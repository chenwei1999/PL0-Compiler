#include "pch.h"
#pragma warning(disable:4996)
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <windows.h>
using namespace std;


/*
   LIT、OPR等是Pcode代码的伪操作码 ，用int类型的0、1等来对应表示 
*/

#define  LIT  0		//LIT 0 ，a 取常量a放入数据栈栈顶
#define  OPR  1		//OPR 0 ，a 执行运算，a表示执行某种运算，具体是何种运算见上面的注释
#define  LOD  2		//LOD L ，a 取变量（相对地址为a，层差为L）放到数据栈的栈顶
#define  STO  3		//STO L ，a 将数据栈栈顶的内容存入变量（相对地址为a，层次差为L）
#define  CAL  4		//CAL L ，a 调用过程（转子指令）（入口地址为a，层次差为L）
#define  INT  5		//INT 0 ，a 数据栈栈顶指针增加a
#define  JMP  6		//JMP 0 ，a无条件转移到地址为a的指令
#define  JPC  7		//JPC 0 ，a 条件转移指令，转移到地址为a的指令
#define  RED  8		//RED L ，a 读数据并存入变量（相对地址为a，层次差为L）
#define  WRT  9		//WRT 0 ，0 将栈顶内容输出
/*
constant、variable、procedure供登陆符号表时使用，代表标识符的类型
*/
#define  constant   0	//常量 
#define  variable   1	//变量
#define  procedure  2	//过程


string a[10] = { "LIT","OPR","LOD","STO","CAL","INT","JMP","JPC","RED","WRT" }; //通过伪操作码来显示指定的操作
int     line = 0;			//词法分析、语法分析时记录出错的行数 
string  strToken = "";		//临时存放标识符or保留字 
char 	ch;					//获取一个字符 
char 	str[10000];			//保存代码 
char    temp[120];			//读文件，存储一行代码 
char 	*p;                 //p=str;
int 	code;				//保留字查询返回码 
void    getsym();           //词法分析获取一个单词
void 	GetChar();          //词法分析获取一个字符
void 	GetBC();            //词法分析用于避开\n \t 空格
void 	Concat();           
bool  	IsLetter();
bool  	IsDigit();
void 	Retract();
void 	ProcError();
int  	Reserve();
void 	prog();				//语法分析 
void    error(int);         //错误处理
void 	block();
void 	condecl();
void 	Const();
void 	vardecl();
void 	proc();
void 	body();
void	statement();
void 	lexp();
void 	exp();
void 	term();
void  	factor();
int     lop();
string  SYM;		//存放种类 
string  ID;			//存放标识符 
int     NUM;		//存放数字 
bool    Error = false;
int     dx=0;		//符号表相对地址
int     tx=0;		//符号表当前下标
int     cx=0;		//Pcode代码当前下标
int     lev;		//层次

//保留字
string  List[30] = { "program","const","var","procedure","begin","end","if","then","else","while",
				  "do","call","read","write","odd",":=",",",";","(",")","=","<>","<","<=",">",">=","+","-","*","/" };

/*
  符号表，以结构体数组的形式存放
*/
struct SymTable
{
	int 	type;		//标识符类型
	int 	value;		//标识符的值
	int 	level;		//标识符所处的层次
	int 	address;    //标识符所处的相对偏移
	int     size;       //标识符大小，默认为4  若标识符为过程定义标识符，此处记录形参个数
	string  name;       //标识符名字
}SymTable[100];

/*
  Pcode代码以结构体形式存放
*/
struct Pcode
{
	int   f;		//伪操作码 
	int   l;		//层差 
	int   a;		//相对地址 
}Pcode[100];



/*
    显示已经生成的Pcode代码
*/
void output()
{
	int i;
	for (i = 0; i < cx; i++)
		cout << a[Pcode[i].f]<< " " << Pcode[i].l<< " " << Pcode[i].a << endl;
}





int I = 0;//指令寄存器I，存放当前要执行的代码
int T = 0; //栈顶指示器T，指向数据栈STACK的栈，栈顶不存放元素
int B = 0; //基址寄存器B，存放当前运行过程的数据区在STACK中的起始地址
int P = 0;//程序地址寄存器，存放下一条要执行的指令的地址
int dataStack[1000];
/*
      根据层差，寻找非局部变量
*/
int getBase(int nowBp, int lev)
{
	int oldBp = nowBp;
	while (lev > 0)//当存在层差时寻找非局部变量
	{
		oldBp = dataStack[oldBp + 1];//直接外层的活动记录首地址
		lev--;
	}
	return oldBp;
}

/*
	解释器
*/
void Interpreter()
{
	P = 0;//程序地址寄存器
	B = 0;//基址寄存器
	T = 0;//栈顶寄存器
	int t;
	do 
	{
		I = P;
		P++;
		switch (Pcode[I].f)//获取伪操作码
		{
		case 0:		//LIT 0 a，取常量a放入数据栈栈顶
			dataStack[T] = Pcode[I].a;
			T++;
			break;
		case 1:     //OPR 0 a，执行运算，a表示执行某种运算
			switch (Pcode[I].a)
			{
			case 0:						//opr,0,0 调用过程结束后，返回调用点并退栈
				T = B;
				P = dataStack[B + 2];	//返回地址
				B = dataStack[B];		//静态链
				break;
			case 1:                 //opr 0,1取反指令
				dataStack[T - 1] = -dataStack[T - 1];
				break;
			case 2:                 //opr 0,2相加，将原来的两个元素退去，将结果置于栈顶
				dataStack[T - 2] = dataStack[T - 1] + dataStack[T - 2];
				T--;
				break;
			case 3:					//OPR 0,3 次栈顶减去栈顶，退两个栈元素，结果值进栈
				dataStack[T - 2] = dataStack[T - 2] - dataStack[T - 1];
				T--;
				break;
			case 4:    				//OPR 0,4次栈顶乘以栈顶，退两个栈元素，结果值进栈
				dataStack[T - 2] = dataStack[T - 1] * dataStack[T - 2];
				T--;
				break;
			case 5:					//OPR 0,5次栈顶除以栈顶，退两个栈元素，结果值进栈
				dataStack[T - 2] = dataStack[T - 2] / dataStack[T - 1];
				T--;
				break;
			case 6:                 //栈顶元素值奇偶判断，结果值进栈,奇数为1
				dataStack[T - 1] = dataStack[T - 1] % 2;
				break;
			case 7:
				break;
			case 8:					//次栈顶与栈项是否相等，退两个栈元素，结果值进栈
				if (dataStack[T - 1] == dataStack[T - 2])
				{
					dataStack[T - 2] = 1;
					T--;
					break;
				}
				dataStack[T - 2] = 0;
				T--;
				break;
			case 9:					//次栈顶与栈项是否不等，退两个栈元素，结果值进栈
				if (dataStack[T - 1] != dataStack[T - 2])
				{
					dataStack[T - 2] = 1;
					T--;
					break;
				}
				dataStack[T - 2] = 0;
				T--;
				break;
			case 10:				//次栈顶是否小于栈顶，退两个栈元素，结果值进栈
				if (dataStack[T - 2] < dataStack[T - 1])
				{
					dataStack[T - 2] = 1;
					T--;
					break;
				}
				dataStack[T - 2] = 0;
				T--;
				break;
			case 11:				//次栈顶是否大于等于栈顶，退两个栈元素，结果值进栈
				if (dataStack[T - 2] >= dataStack[T - 1])
				{
					dataStack[T - 2] = 1;
					T--;
					break;
				}
				dataStack[T - 2] = 0;
				T--;
				break;
			case 12:				//次栈顶是否大于栈顶，退两个栈元素，结果值进栈
				if (dataStack[T - 2] > dataStack[T - 1])
				{
					dataStack[T - 2] = 1;
					T--;
					break;
				}
				dataStack[T - 2] = 0;
				T--;
				break;
			case 13:				//次栈顶是否小于等于栈顶，退两个栈元素，结果值进栈
				if (dataStack[T - 2] <= dataStack[T - 1])
				{
					dataStack[T - 2] = 1;
					T--;
					break;
				}
				dataStack[T - 2] = 0;
				T--;
				break;
			case 15:				//屏幕输出换行
				cout << endl;
				break;
			}
			break;
		case 2:  //LOD L ，a 取变量（相对地址为a，层差为L）放到数据栈的栈顶
			dataStack[T] = dataStack[Pcode[I].a + getBase(B, Pcode[I].l)];
			T++;
			break;
		case 3://STO L ，a 将数据栈栈顶的内容存入变量（相对地址为a，层次差为L）
			dataStack[Pcode[I].a + getBase(B, Pcode[I].l)] = dataStack[T - 1];
			//T--;
			break;
		case 4:		//CAL L ，a 调用过程（转子指令）（入口地址为a，层次差为L）
			dataStack[T] = B;		//静态链，直接外层过程
			dataStack[T + 1] = getBase(B, Pcode[I].l);	//动态链，调用前运行过程
			dataStack[T + 2] = P;		//返回地址，下一条要执行的
			B = T;
			P = Pcode[I].a;
			break;
		case 5:						//INT 0 ，a 数据栈栈顶指针增加a
			T = T + Pcode[I].a;
			break;
		case 6:						//JMP 0 ，a无条件转移到地址为a的指令
			P = Pcode[I].a;
			break;
		case 7:						//JPC 0 ，a 条件转移指令，转移到地址为a的指令
			if (dataStack[T - 1] == 0)
			{
				P = Pcode[I].a;
			}
			break;
		case 8:					//从命令行读入一个置于栈顶
			cin >>t;
			dataStack[Pcode[I].a + getBase(B, Pcode[I].l)]=t;
			break;
		case 9:					//栈顶值输出至屏幕
			cout<<dataStack[T-1];
			cout<<"  ";    
			break;
		}
	}while (P != 0);
}






int main()
{

	fstream dataFile;
	string filename;
	char choice;
	cout << "*******************161710223陈伟*****************" << endl;
	cout << "请输入要编译的文件名(包括文件后缀):";
	cin >> filename;
	getchar();
	//输入要编译的文件
	dataFile.open(filename, ios::in);
	if (!dataFile)
	{
		cout << "Failed to read source program!\n";
		exit(0);
	}
	//将所有代码读入到str数组，包括换行符
	dataFile.getline(temp, 120);
	strcat(str, temp);
	strcat(str, "\n");    //以换行符判断行数 
	while (!dataFile.eof())
	{
		dataFile.getline(temp, 120);
		strcat(str, temp);
		strcat(str, "\n");
	}
	strcat(str, "\0");
	line++;    //初始值为1(行)
	p = str;
	SYM = "";  
	ID = "";
	NUM = 0;
	cout << "生成目标代码中······" << endl;
	Sleep(2000);
	getsym();  //词法分析
	prog();
	if (Error)
		cout << endl << endl << "complie error" << endl;
	else
	{
		cout << "complie success" << endl;
		cout << "********************************" << endl;
		cout << "目标代码:" << endl;
		output();
		cout << "是否解释执行代码(是:Y 否:N):";
		cin >> choice;
		if (choice == 'Y'|| choice == 'y')
			Interpreter();
		else
			exit(0);
	}
	dataFile.close();
}


/*
  往符号表填入变量标识符
*/
void entervar(string name,int level,int dx)
{
	tx = tx + 1;
	SymTable[tx].name =name;
	SymTable[tx].type = 1;
	SymTable[tx].level = lev;
	SymTable[tx].address = dx;
}


/*
  往符号表填入常量标识符
*/
void enterconst(string name, int level, int value,int dx)
{
	tx = tx + 1;
	SymTable[tx].name = name;
	SymTable[tx].type = 0;
	SymTable[tx].value = NUM;
	SymTable[tx].level = lev;
	SymTable[tx].address = dx;
}

/*
  往符号表填入过程标识符
*/
void enterproc(string name, int level, int dx)
{
	tx = tx + 1;
	SymTable[tx].name = name;
	SymTable[tx].type = 2;
	SymTable[tx].level = lev;
	SymTable[tx].address = dx;
}

/*
   查找符号表中标识符的位置
*/
int position(string name)
{
	int i;
	SymTable[0].name =name;
	i = tx;
	while (SymTable[i].name != name)
	{
		i--;
	}
	return i;
}

/*
   查找符号表中过程定义的标识符
*/
int searchproc()
{
	int i;
	for (i = tx; i >= 1; i--)
	{
		if (SymTable[i].type == 2)
		{
			return i;
		}
	 }
	return -1;
}

/*
	 查找标识符是否定义，同层或外层
*/
bool isPreExistSTable(string name,int level)
{
	int i;
	for (i = 1; i <= tx; i++)
	{
		if (SymTable[i].name == name)
		{
			if (SymTable[i].level == level)     //查找本层的
			{
				return true;
			 }
			else
			{
				do 
				{
					level = level - 1;               //查找外层的
					if (SymTable[i].level == level)   
					{
						return true;
					}
				} while (level >= 0);
			}
		 }
	}
	return false;
}

/*
     查找标识符在同层次中是否定义
*/

bool isNowExistSTable(string name,int level)
{
	int i;
	for (i = 1; i <= tx; i++)
	{
		if (SymTable[i].name == name&& SymTable[i].level == level)
		{
			return true;
		}
	}
	return false;
}




/*
   Pcode代码生成程序
*/
void gen(int f, int l, int a)
{
	Pcode[cx].f = f;
	Pcode[cx].l = l;
	Pcode[cx].a = a;
	cx++;
}

/*
     语法分析出错处理程序
*/
void error(int n)
{
	Error = true;
	switch (n)
	{
		case  -1:cout << "[ERROR]" << "index " << line << ":" << "Missing var/const" << endl; break;
		case  0:cout << "[ERROR]" << "index " << line << ":" << "Missing  ;" << endl; break;
		case  1:cout << "[ERROR]" << "index " << line << ":" << "Identifier illegal" << endl; break;
		case  2:cout << "[ERROR]" << "index " << line << ":" << "Missing  program" << endl; break;
		case  3:cout << "[ERROR]" << "index " << line << ":" << "Missing  :=" << endl; break;
		case  4:cout << "[ERROR]" << "index " << line << ":" << "Missing  (" << endl; break;
		case  5:cout << "[ERROR]" << "index " << line << ":" << "Missing  )" << SYM << endl;
		case  6:cout << "[ERROR]" << "index " << line << ":" << "Missing  Begin" << endl; break;
		case  7:cout << "[ERROR]" << "index " << line << ":" << "Missing  End" << endl; break;
		case  8:cout << "[ERROR]" << "index " << line << ":" << "Missing  Then" << endl; break;
		case  9:cout << "[ERROR]" << "index " << line << ":" << "Missing do" << endl; break;
		case 10:cout << "[ERROR]" << "index " << line << ":" << "Not exist " <<ID<<endl; break;
		case 11:cout << "[ERROR]" << "index " << line << ":" <<ID<<"is not a procedure" << endl; break;
		case 12:cout << "[ERROR]" << "index " << line << ":" <<ID<< "is not a variable" << endl; break;
		case 13:cout << "[ERROR]" << "index " << line << ":" <<ID << "is not a variable" << endl; break;
		case 14:cout << "[ERROR]" << "index " << line << ":" <<"Not exist " <<ID<< endl; break;
		case 15:cout << "[ERROR]" << "index " << line << ":" << "Duplicate definition  " << ID << endl; break;
		case 16:cout << "[ERROR]" << "index " << line << ":" << "The number of parameters does not match" << endl; break;
		case 17:cout << "[ERROR]" << "index " << line << ":" << "ILLEGAL" << endl; break;
		case 18:cout << "[ERROR]" << "index " << line << ":" << "Missing Var" << endl; break;
	}

}






// <prog> → program <id>；<block>

void prog()
{
	if (SYM == "program")
	{
		getsym();
		if (SYM != "id")
		{
			error(1);
		}
		else
		{
			getsym();
			if (SYM != ";")
			{
				error(0);
				return;
			}
			else
			{
				getsym();
				block();
			}
		}

	}
	else
	{
		error(2);
		return;
	}
}




//<block> → [<condecl>][<vardecl>][<proc>]<body>

void block()
{
	int dx0 = dx;             //记录当前的层次，以便恢复时使用
	int tx0 = tx + 1;        //记录符号表当前等待填入的位置
	int cx0;
	int n=0;
	if (tx0 > 1)             
	{
		n = searchproc();                 //寻找过程标识符
		tx0 = tx0 - SymTable[n].size;     //减去形参的个数，恢复时形参也不保留
	}
	if (tx0 == 1)
	{
		dx = 3;                        //静态链、动态链、返回地址
	}
	else
	{
		dx = 3 + SymTable[n].size;    //+形参的个数，形参置于静态链、动态链、返回地址和变量、常量之前
	}
	cx0 = cx;                         //记录跳转指令的位置，等待下次回填
	gen(JMP, 0, 0);

	if (SYM != "const"&&SYM != "var"&&SYM != "procedure"&&SYM != "begin")
	{
		error(17);
		return ;
	}
	if (SYM == "const")
		condecl();
	if (SYM == "var")
		vardecl();
	if (SYM == "procedure")
	{
		proc();
		lev--;          //出嵌套，层次-1
	}
	if (tx0 > 1)
	{
		int i;
		
		n = searchproc();

		for (i = 0; i <SymTable[n].size ; i++)
		{
			gen(STO, 0, SymTable[n].size + 3 -1-i);   //将实参的值传给形参
		}
	}
	Pcode[cx0].a = cx;      //回填JMP指令
	gen(INT, 0, dx);        //开辟dx个空间
	if(tx!=1)
	  SymTable[n].value = cx - 1 - SymTable[n].size;    //将过程入口地址填入过程定义的value，以便call指令使用
	body();
	gen(OPR, 0, 0);     
	tx = tx0;                       //清符号表，将符号表指针往前移至tx0处
	dx = dx0;
}



//<condecl> → const <const>{,<const>};
void condecl()
{
	if (SYM == "const")
	{
		getsym();
		Const();
		while (SYM == ",")
		{
			getsym();
			Const();
		}
		if (SYM == ";")
		{
			getsym();
			return;
		}
		else
		{
			error(0);
		}

	}
	else
	{
		error(-1);
	}
}

//<const> → <id>:=<integer>
void Const()
{
	string name;
	int value;
	if (SYM == "id")
	{
		name = ID;
		getsym();
		if (SYM == ":=")
		{
			getsym();
			if (SYM == "integer")
			{
				value = NUM;
				if (isNowExistSTable(name, lev))        
				{
				   error(15);
				}
				enterconst(name, lev, value, dx);
				getsym();
			}
		}
		else
		{
			error(3);
			return;
		}
	}
	else
	{
		error(1);
		return;
	}
}

//<vardecl> → var <id>{,<id>};
void vardecl()
{
	string name;
	if (SYM == "var")
	{
		getsym();
		if (SYM == "id")
		{
			name = ID;
			if (isNowExistSTable(name, lev))
			{
				error(15);
			}
			entervar(name, lev, dx);
			dx++;
			getsym();
			while (SYM == ",")
			{
				getsym();
				if (SYM == "id")
				{
					name = ID;
					if (isNowExistSTable(name, lev))
					  {
						 error(15);
                       }
					entervar(name, lev, dx);
					dx++;
					getsym();
				}
				else
				{
					error(1);
				}
			}
			if (SYM != ";")
			{
				error(0);
				return;
			}
			else
				getsym();
		}
		else
		{
			error(1);
			return;
		}

	}
	else
	{
		error(-1);
		return;
	}
}


//<proc> → procedure <id>[（<id>{,<id>}）];<block>{;<proc>}
void proc()
{
	if (SYM == "procedure")
	{
		getsym();
		int count = 0;
		int tx0;
		if (SYM == "id")
		{
			string name = ID;
			if (isNowExistSTable(name, lev))
			{
				error(15);
			}
			tx0 = tx + 1;
			enterproc(name, lev, dx);
			lev++;
			getsym();
			if (SYM == "(")
			{
				getsym();
				if (SYM == "id")
				{
					entervar(ID, lev, 3 + count);
					count++;                             //形参个数加+1
					SymTable[tx0].size = count;
					getsym();
					while (SYM == ",")
					{
						getsym();
						if (SYM == "id")
						{
							entervar(ID, lev, 3 + count);
							count++;
							SymTable[tx0].size = count;
							getsym();
						}	
						else
						{
							error(1);
							return;
						}
					}
				}
				if (SYM == ")")
				{
					getsym();
					if (SYM != ";")
					{
						error(0);
						return;
					}
					else
					{
						getsym();
						block();
						while (SYM == ";")
						{
							getsym();
							proc();
						}
					}

				}
				else
				{
					error(5);
					return;
				}
			}
			else
			{
				error(4);
				return;
			}
		}
		else
		{
			error(1);
			return;
		}
	}
	else
	{
		error(-1);
		return;
	}

}


//<body> → begin <statement>{;<statement>}end
void body()
{
	if (SYM == "begin")
	{
		getsym();
		statement();
		while (SYM == ";"&&SYM!="end")
		{
			if (SYM == ";")
			{
				getsym();
				statement();
			}
			else
			{
				error(0);
				return;
			}
		}
		if (SYM == "end")
		{
			getsym();
		}
		else
		{
			error(7);
			return;
		}
	}
	else
	{
		error(6);
		return;
	}

	
}

/*<statement> → <id> := <exp>
			   |if <lexp> then <statement>[else <statement>]
			   |while <lexp> do <statement>
			   |call <id>（[<exp>{,<exp>}]）
			   |<body>
			   |read (<id>{，<id>})
			   |write (<exp>{,<exp>})
*/
void statement()
{
	if (SYM == "if")
	{
		int cx1;
		getsym();
		lexp();
		if (SYM == "then")
		{
			cx1 = cx;
			gen(JPC, 0, 0);
			getsym();
			statement();
			int cx2 = cx;
			gen(JMP, 0, 0);
			Pcode[cx1].a = cx;
			Pcode[cx2].a = cx;
			if (SYM == "else")
			{
				getsym();
				statement();
				Pcode[cx2].a = cx;
			}

		}
		else
		{
			error(8);
			return;
		}
	}

	else  if (SYM == "while")
	{
		int cx1 = cx;
		getsym();
		lexp();
		if (SYM == "do")
		{
			int cx2 = cx;
			gen(JPC, 0, 0);
			getsym();
			statement();
			gen(JMP, 0, cx1);
			Pcode[cx2].a = cx;
		}
		else
		{
			error(9);
			return;
		}
	}
	else if (SYM == "write")
	{
		getsym();
		if (SYM == "(")
		{
			getsym();
			exp();
			gen(WRT, 0, 0);
			while (SYM == ",")
			{
				getsym();
				exp();
				gen(WRT, 0, 0);
			}
			gen(OPR, 0, 15);
			if (SYM == ")")
			{
				getsym();
			}
			else
			{
				error(5);
				return;
			}
		}
		else
		{
			error(4);
			return;
		}
	}
	else if (SYM == "begin")
	{
		body();
	}
	else if (SYM == "id")
	{
		string name = ID;
		getsym();
		if (SYM == ":=")
		{
			getsym();
			exp();
			if (!isPreExistSTable(name, lev))
			{
				error(14);
				return;
			}
			else
			{
				int i = position(name);
				if (SymTable[i].type == 1)
					gen(STO, lev - SymTable[i].level, SymTable[i].address);
				else
				{
					error(13);
					return;
				}
			}

		}
		else
		{
			error(3);
			return;
		}
	}
	else if (SYM == "read")
	{
	getsym();
	if (SYM == "(")
	{
		getsym();
		if (SYM == "id")
		{
			if (!isPreExistSTable(ID, lev))
			{
				error(10);
				return;
			}
			else
			{
				int i = position(ID);
				if (SymTable[i].type == 1)
				{
					gen(RED, lev - SymTable[i].level, SymTable[i].address);
				}
				else
				{
					error(12);
					return;
				}
			}
			getsym();
			while (SYM == ",")
			{
				getsym();
				if (SYM == "id")
				{
					if (!isPreExistSTable(ID, lev))
					{
						error(10);
						return;
					}
					else
					{
						int i = position(ID);
						if (SymTable[i].type == 1)
						{
							gen(RED, lev - SymTable[i].level, SymTable[i].address);
						}
						else
						{
							error(12);
							return;
						}
					}
					getsym();
				}
				else
				{
					error(1);
					return;
				}
			}
			if (SYM == ")")
			{
				getsym();
			}
			else
			{
				error(5);
			}
		}
		else
		{
			error(18);
		}
	}
	else
	{
		error(4);
		return;
	}
	}
	else if(SYM=="call")
	{
		
	    getsym();
		int count = 0,i;
		if (SYM =="id")
		{
			if (!isPreExistSTable(ID, lev))
			{
				error(10);
				return;
			}
			else
			{
			    i = position(ID);
				if (SymTable[i].type == 2)
				{
					
				}
				else
				{
					error(11);
					return;
				}
			}
			getsym();
			if (SYM == "(")
			{
				getsym();
				if (SYM == ")")
				{
					getsym();
					gen(CAL, lev - SymTable[i].level, SymTable[i].value);
				}
				else
				{
					exp();
					count++;
					while (SYM == ",")
					{
						getsym();
						exp();
						count++;
					}
					if (count != SymTable[i].size)
					{
						error(16);
						return;
					}
					gen(CAL, lev - SymTable[i].level, SymTable[i].value);
						if (SYM == ")")
						{
							getsym();
						}
						else
						{
							error(5);
							return;
						}
				}
			}
			else
			{
				error(4);
				return;
			}
		}
		else
		{
			error(1);
			return;
		}







     
}




	else
	{
	error(1);
	return;
 }

}


//<lexp> → <exp> <lop> <exp>|odd <exp>
void lexp()
{
	if (SYM == "odd")
	{
		getsym();
		exp();
		gen(OPR, 0, 6);
	}
	else
	{
		exp();
		int i=lop();
		exp();
		if (i == 0)
		{
			gen(OPR, 0, 8);
		}
		else if (i == 1)
		{
			gen(OPR, 0, 9);
		}
		else if (i == 2)
		{
			gen(OPR, 0, 10);
		}
		else if (i == 3)
		{
			gen(OPR, 0, 13);
		}
		else if (i == 4)
		{
			gen(OPR, 0, 12);
		}
		else if (i == 5)
		{
			gen(OPR, 0, 11);
		}

	}

}


//<exp> → [+|-]<term>{<aop><term>}
void exp()
{
	string temp = SYM;
	if (SYM == "+" || SYM == "-")
	{
		getsym();
	}
	term();
	if (temp == "+")
	{
		gen(OPR, 0, 1);
	}
	while (SYM == "+" || SYM == "-")
	{
		temp = SYM;
		getsym();
		term();
		if (temp == "+")
		{
			gen(OPR, 0, 2);
		}
		else if(temp == "-")
		{
			gen(OPR, 0, 3);
		}
	}

}


//<term> → <factor>{<mop><factor>}
void term()
{

	factor();
	while (SYM == "*" || SYM == "/")
	{
		string  temp = SYM;
		getsym();
		factor();
		if (temp == "*")
		{
			gen(OPR, 0, 4);
		}
		else if (temp == "/")
		{
			gen(OPR, 0, 5);
		}
	}

}

//<factor>→<id>|<integer>|(<exp>)
void  factor()
{
	if (SYM=="integer")
	{
		gen(LIT, 0, NUM);
		getsym();
	 }
	else if (SYM == "(")
	{
		getsym();
		exp();
		if (SYM == ")")
		{
			getsym();
		}
		else
		{
			error(5);
		}
	}
	else if(SYM=="id")
	{
		string name = ID;
		if (!isPreExistSTable(name, lev))
		{
			error(10);
			return;
		}
		else
		{
			int i = position(name);
			if(SymTable[i].type==1)
			  gen(LOD, lev - SymTable[i].level, SymTable[i].address);
			else if (SymTable[i].type == 0)
			{
				gen(LIT, 0, SymTable[i].value);
			}
			else
			{
				error(12);
				return;
			}
		}
		getsym();
	}
	else
	{
		error(1);
	}
}

int lop()
{
	if (SYM == "=")
	{
		getsym();
		return 0;
	}
	else if (SYM == "<>")
	{
		getsym();
		return 1;
	}
	else if (SYM == "<")
	{
		getsym();
		return 2;
	}
	else if (SYM == "<=")
	{
		getsym();
		return 3;
	}
	else if (SYM == ">")
	{
		getsym();
		return 4;
	}
	else if (SYM == ">=")
	{
		getsym();
		return 5;
	}
	return -1;

}

/*
   词法分析
*/
void getsym()
{
	strToken = "";
	GetChar();
	GetBC();
	if (IsLetter())
	{
		while (IsLetter() || IsDigit())   
		{
			Concat();
			GetChar();
		}
		Retract();
		code = Reserve(); 
		if (code == -1)          //判断标识符是否为保留字
		{
			SYM = "id";
			ID = strToken;
		}
		else
		{
			SYM = List[code];  
		}
		return;
	}
	else if (IsDigit())
	{
		int sum = 0;
		while (IsDigit())
		{
			Concat();
			sum = sum * 10 + (ch - '0');   //将字符串数字转换为int
			GetChar();
		}
		if (IsLetter())                               //首字符是数字字符，后接字母字符为错
		{
			while (IsLetter() || IsDigit())
			{
				Concat();
				GetChar();
			}
			Retract();
			ProcError();
		}
		else
		{
			SYM = "integer";
			NUM = sum;
			Retract();
			return;
		}
	}
	else if (ch == ':')
	{
		Concat();
		GetChar();
		if (ch == '=')
		{
			SYM = ":=";
			return;
		}
		else
		{
			ProcError();        
		}
	}
	else if (ch == '+')
	{
		SYM = "+";
		return;
	}
	else if (ch == '-')
	{
		SYM = "-";
		return;
	}
	else if (ch == '=')
	{
		SYM = "=";
		return;
	}
	else if (ch == '(')
	{
		SYM = "(";
		return;
	}
	else if (ch == ')')
	{
		SYM = ")";
		return;
	}
	else if (ch == '*')
	{
		SYM = "*";
		return;
	}
	else if (ch == '/')
	{
		SYM = "*";
		return;
	}
	else if (ch == ';')
	{
		SYM = ";";
		return;
	}
	else if (ch == ',')
	{
		SYM = ",";
		return;
	}
	else if (ch == '<')
	{
		GetChar();
		if (ch == '=')
		{
			SYM = "<=";
			return;
		}
		if (ch == '>')
		{
			SYM = "<>";
			return;
		}
		Retract();
		SYM = "<";
		return;
	}
	else if (ch == '>')
	{
		GetChar();
		if (ch == '=')
		{
			SYM = ">=";
			return;
		}
		Retract();
		SYM = ">";
		return;
	}
	else if(ch=='.')
	{

     }
	else
	{
		Concat();
		ProcError();
	}
}


/*
    获取下一个字符
*/
void GetChar()
{
	if (*p == '\0')
		ch = '.';
	else
		ch = *p;

	p++;
}

/*
   忽略空格、换行、制表符
*/
void GetBC()
{
	while (ch == ' ' || ch == '\n' || ch == '\t')
	{
		if (ch == '\n')
			line++;
		GetChar();
	}
}


/*
    字符串拼接
*/
void Concat()
{
	strToken += ch;
}


/*
 判断字符是否为字母
*/
bool IsLetter()
{
	if ((ch >= 'a'&&ch <= 'z') || (ch >= 'A'&&ch <= 'Z'))
		return true;
	return false;
}


/*
   判断字符是否为数字
*/
bool IsDigit()
{
	if (ch >= '0'&&ch <= '9')
		return true;
	return false;
}



/*
     词法分析时将指针前移1位
*/
void Retract()
{
	p--;
	ch = ' ';
}



/*
    查找标识符是否为保留字，是则返回保留字数组的对应下标
*/
int Reserve()
{
	int i;
	for (i = 0; i < 15; i++)
		if (strToken == List[i])
			break;
	if (i == 15)
		return -1;
	else
		return i;
}


/*
   词法分析的出错处理函数
*/
void ProcError()
{
	cout << "[ERROR]" << "index " << line << ":" << "unknow character" << "'" << strToken << "'" << endl;
}


