

@REM  "-zo 为每个函数生成一个section"
@REM  "ropi 生成pic 位置无关代码"
@REM  "rwpi 生成pid 位置无关数据"
@REM  "interwork 生成能在arm与thumb混合使用的代码"
@REM  "-fa 检查局部变量是否未初始化就使用了"

armcc -I. -c -O2 -Otime -cpu ARM7EJ-S -littleend -apcs /ropi/rwpi/interwork -fa -zo -o tmp/main.o src/main.c

armcc -I. -c -O2 -Otime -cpu ARM7EJ-S -littleend -apcs /ropi/rwpi/interwork -fa -zo -o tmp/r9.o src/r9.c

armcc -I. -c -O2 -Otime -cpu ARM7EJ-S -littleend -apcs /interwork -fa -zo -o tmp/lib.o src/lib.c

@REM " armlink -rwpi #使加载和执行RW ZI位置无关"
@REM "    -ro-base 0x80000  #设置RO加载和执行的地址"
@REM "    -remove # 删除未使用的部分"
@REM "    -first mr_c_function_load "
@REM "    -entry mr_c_function_load "

@REM "    -map "
@REM "    -info sizes,totals,veneers #输出信息"
@REM "    -xref 此选项列出输入部分之间的所有交叉引用"
@REM "    -symbols 此选项列出链接步骤中使用的每个局部和全局符号及其值。这包括链接器生成的符号。"
@REM "    -list tmp/cfunction.txt  输出-map -info -xref -symbols的信息到文件"
@REM "    -o tmp/cfunction.elf tmp/main.o tmp/r9.o tmp/lib.o"

armlink -rwpi -ro-base 0x80000 -remove -first mr_c_function_load -entry mr_c_function_load -map -info sizes,totals,veneers -xref -symbols -list tmp/cfunction.txt -o tmp/cfunction.elf tmp/main.o tmp/r9.o tmp/lib.o

pause