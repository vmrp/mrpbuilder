package main

import (
	"bytes"
	"compress/gzip"
	"debug/elf"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"hash/crc32"
	"io"
	"io/ioutil"
	"os"
	"path"
	"strings"

	"golang.org/x/text/encoding/simplifiedchinese"
)

const elfToExt = false

func paif(err error) {
	if err != nil {
		panic(err)
	}
}

func gzipFile(file string) []byte {
	inFile, err := os.Open(file)
	paif(err)
	defer inFile.Close()

	var buf bytes.Buffer
	zw := gzip.NewWriter(&buf)
	_, err = io.Copy(zw, inFile)
	paif(err)
	err = zw.Close()
	paif(err)
	return buf.Bytes()
}

// ext文件实际是fmt(elf)文件仅保留ro/rw/zi段，然后在开始处增加8字节MRPGCMAP
// 中间要保留下来的长度这样得到：
// Program header在文件开头偏移量28的4字节记录
// 这个偏移量再加上16得到一个新的偏移量，在这个位置的4字节就是代码的长度
// 最后删除开始52字节(elf头)和保留长度之后的东西
func toExt(elfFileName string, outputFileName string) {
	fmt.Println(elfFileName, "->", outputFileName)
	ef, err := elf.Open(elfFileName)
	paif(err)
	if len(ef.Progs) != 1 {
		panic(errors.New("len(ef.Progs) != 1"))
	}
	filesz := ef.Progs[0].Filesz
	ef.Close()

	f, err := os.Open(elfFileName)
	paif(err)
	defer f.Close()

	_, err = f.Seek(52, os.SEEK_SET)
	paif(err)

	buf := make([]byte, filesz)
	n, err := f.Read(buf)
	paif(err)
	if n != len(buf) {
		panic(errors.New("n != len(buf)"))
	}
	buf = append([]byte("MRPGCMAP"), buf...)
	err = ioutil.WriteFile(outputFileName, buf, 0666)
	paif(err)
}

type MRPHeader struct {
	Magic          [4]byte  // [0:4]     固定标识'MRPG'
	FileStart      uint32   // [4:8]     文件头的长度+文件列表的长度-8
	MrpTotalLen    uint32   // [8:12]    mrp文件的总长度
	MRPHeaderSize  uint32   // [12:16]   文件头的长度，通常是240，如果有额外数据则需要加上额外数据的长度
	FileName       [12]byte // [16:28]   GB2312编码带'\0'
	DisplayName    [24]byte // [28:52]   GB2312编码带'\0'
	AuthStr        [16]byte // [52:68]   编译器的授权字符串的第2、4、8、9、11、12、1、7、6位字符重新组合的一个字符串
	Appid          uint32   // [68:72]
	Version        uint32   // [72:76]
	Flag           uint32   // [76:80]   第0位是显示标志， 1-2位是cpu性能要求，所以cpu取值范围是0-3只对展讯有效， 第3位是否是shell启动的标志，0表示start启动，1表示shell启动
	BuilderVersion uint32   // [80:84]   应该是编译器的版本，从几个mrpbuilder看都是10002
	Crc32          uint32   // [84:88]   整个文件计算crc后写回，计算时此字段的值为0
	Vendor         [40]byte // [88:128]  GB2312编码带'\0'
	Desc           [64]byte // [128:192] GB2312编码带'\0'
	AppidBE        uint32   // [192:196] 大端appid
	VersionBE      uint32   // [196:200] 大端version
	Reserve2       uint32   // [200:204] 保留字段
	ScreenWidth    uint16   // [204:206] 在反编译的mrpbuilder中能看到有屏幕信息的字段，但是在斯凯提供的文档中并没有说明
	ScreenHeight   uint16   // [206:208]
	Plat           uint8    // [208:209] mtk/mstar填1，spr填2，其它填0
	Reserve3       [31]byte // [209:240]
	// ...       额外的数据，通常情况下没有
}

func printHeader(data *MRPHeader) {
	fmt.Println("Magic:", string(data.Magic[:]))
	fmt.Println("FileStart:", data.FileStart)
	fmt.Println("MrpTotalLen:", data.MrpTotalLen)
	fmt.Println("MRPHeaderSize:", data.MRPHeaderSize)
	fmt.Println("FileName:", string(data.FileName[:]))
	fmt.Println("DisplayName:", GBKToUTF8(data.DisplayName[:]))
	fmt.Println("AuthStr:", data.AuthStr)
	fmt.Println("Appid:", data.Appid)
	fmt.Println("Version:", data.Version)
	fmt.Println("Flag:", data.Flag)
	fmt.Println("BuilderVersion:", data.BuilderVersion)
	fmt.Println("Crc32:", data.Crc32)
	fmt.Println("Vendor:", GBKToUTF8(data.Vendor[:]))
	fmt.Println("Desc:", GBKToUTF8(data.Desc[:]))
	fmt.Println("AppidBE:", data.AppidBE)
	fmt.Println("VersionBE:", data.VersionBE)
	fmt.Println("ScreenWidth:", data.ScreenWidth)
	fmt.Println("ScreenHeight:", data.ScreenHeight)
	fmt.Println("Plat:", data.Plat)

	fmt.Println("test Appid BE to LE:", BigEndianToLittleEndian(data.AppidBE))
	fmt.Println("test Appid LE to BE:", LittleEndianToBigEndian(data.Appid))
}

func GBKToUTF8(bts []byte) string {
	var dec = simplifiedchinese.GBK.NewDecoder()
	r, err := dec.Bytes(bts)
	paif(err)
	return string(r)
}

func UTF8ToGBK(str string) []byte {
	var enc = simplifiedchinese.GBK.NewEncoder()
	bts, err := enc.Bytes([]byte(str))
	paif(err)
	return bts
}

func BigEndianToLittleEndian(v uint32) uint32 {
	a := make([]byte, 4)
	binary.BigEndian.PutUint32(a, v)
	return binary.LittleEndian.Uint32(a)
}

func LittleEndianToBigEndian(v uint32) uint32 {
	a := make([]byte, 4)
	binary.LittleEndian.PutUint32(a, v)
	return binary.BigEndian.Uint32(a)
}

func getUint32Data(v uint32) []byte {
	a := make([]byte, 4)
	binary.LittleEndian.PutUint32(a, v)
	return a
}

type Config struct {
	Display     string   `json:"display"`
	FileName    string   `json:"filename"`
	Appid       uint32   `json:"appid"`
	Version     uint32   `json:"version"`
	Vendor      string   `json:"vendor"`
	Description string   `json:"description"`
	Visible     uint32   //默认1
	CPU         uint32   //默认1
	Shell       uint32   //默认0
	Files       []string `json:"files"`
}

type FileList struct {
	FileNameLen uint32
	FileName    []byte
	FilePos     uint32
	FileLen     uint32
	Unknown     uint32
	FileData    []byte
}

func initHeader(header *MRPHeader, config *Config) {
	var flag uint32 = 1
	if config.Visible == 0 {
		flag = 0
	}
	flag = flag + (config.CPU&0b11)<<1
	flag = flag + (config.Shell&0b1)<<3

	header.Magic = [4]byte{'M', 'R', 'P', 'G'}
	header.MRPHeaderSize = 240 // todo 这个值不能固定死，需要计算出来，因为文件头还可以附带额外的数据
	header.Appid = config.Appid
	header.Version = config.Version
	header.Flag = flag
	header.BuilderVersion = 10002
	header.AppidBE = LittleEndianToBigEndian(config.Appid)
	header.VersionBE = LittleEndianToBigEndian(config.Version)
	header.Plat = 1

	_, filename := path.Split(config.FileName)
	tmp := UTF8ToGBK(filename)
	if len(tmp) > 11 {
		paif(errors.New("FileName.length > 11"))
	}
	for i, v := range tmp {
		header.FileName[i] = v
	}

	tmp = UTF8ToGBK(config.Display)
	if len(tmp) > 23 {
		paif(errors.New("Display.length > 11"))
	}
	for i, v := range tmp {
		header.DisplayName[i] = v
	}

	tmp = UTF8ToGBK(config.Vendor)
	if len(tmp) > 39 {
		paif(errors.New("Vendor.length > 11"))
	}
	for i, v := range tmp {
		header.Vendor[i] = v
	}

	tmp = UTF8ToGBK(config.Description)
	if len(tmp) > 23 {
		paif(errors.New("Description.length > 11"))
	}
	for i, v := range tmp {
		header.Desc[i] = v
	}
}

func main() {
	var config Config
	var header MRPHeader

	config.CPU = 3
	config.Visible = 1
	config.Shell = 0

	bts, err := ioutil.ReadFile("pack.json")
	paif(err)
	err = json.Unmarshal(bts, &config)
	paif(err)

	initHeader(&header, &config)

	fileList := make([]FileList, len(config.Files))
	var listLen, dataLen uint32
	for i, v := range config.Files {
		if elfToExt {
			if strings.ToLower(path.Ext(v)) == ".elf" {
				ext := strings.Replace(v, ".elf", ".ext", -1)
				toExt(v, ext)
				v = ext
			}
		}

		_, file := path.Split(v)
		// []byte(file) 转换出来的slice是没有'\0'结尾的
		tmp := append([]byte(file), 0x00)
		listItem := &fileList[i]
		listItem.FileName = tmp
		listItem.FileNameLen = uint32(len(tmp))
		listItem.FileData = gzipFile(v)
		listItem.FileLen = uint32(len(listItem.FileData))
		// 每个列表项中由文件名长度、文件名、文件偏移、文件长度、0 组成，数值都是uint32因此需要4*4
		listLen += listItem.FileNameLen + 4*4
		dataLen += listItem.FileNameLen + 4*2 + listItem.FileLen
	}
	// 第一个文件数据的开始位置
	var filePos uint32 = header.MRPHeaderSize + listLen
	header.FileStart = filePos - 8 // 不明白为什么要减8，但是必需这样做
	header.MrpTotalLen = header.MRPHeaderSize + listLen + dataLen

	buf := new(bytes.Buffer)
	// 写文件头
	err = binary.Write(buf, binary.LittleEndian, &header)
	paif(err)

	// 写出文件列表
	for i := range fileList {
		listItem := &fileList[i]
		// 每个文件数据由：文件名长度、文件名、文件大小组成，数值都是uint32因此需要4*2
		filePos += listItem.FileNameLen + 4*2
		listItem.FilePos = filePos
		// 下一个文件数据的开始位置
		filePos += listItem.FileLen

		buf.Write(getUint32Data(listItem.FileNameLen))
		buf.Write(listItem.FileName)
		buf.Write(getUint32Data(listItem.FilePos))
		buf.Write(getUint32Data(listItem.FileLen))
		buf.Write(getUint32Data(0))
		fmt.Printf("filename: %s \t pos: %d \t len: %d\n", string(listItem.FileName), listItem.FilePos, listItem.FileLen)
	}

	// 写出文件数据
	for i := range fileList {
		listItem := &fileList[i]
		buf.Write(getUint32Data(listItem.FileNameLen))
		buf.Write(listItem.FileName)
		buf.Write(getUint32Data(listItem.FileLen))
		buf.Write(listItem.FileData)
	}

	bts = buf.Bytes()

	if len(bts) != int(header.MrpTotalLen) {
		fmt.Println("Write Data Fail")
		return
	}

	crc := crc32.Checksum(bts, crc32.MakeTable(crc32.IEEE))

	mrpf, err := os.Create(config.FileName)
	paif(err)
	defer mrpf.Close()

	mrpf.Write(bts[:84])
	mrpf.Write(getUint32Data(crc))
	mrpf.Write(bts[88:])

	fmt.Println("done.")
}

func test() {
	buf, err := ioutil.ReadFile("asm.mrp")
	paif(err)

	var data MRPHeader
	if err := binary.Read(bytes.NewReader(buf), binary.LittleEndian, &data); err != nil {
		fmt.Println("binary.Read failed:", err)
	}
	printHeader(&data)
}
