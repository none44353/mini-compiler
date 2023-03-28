%code requires {
  #include <memory>
  #include <string>
}

%{

#include <iostream>
#include <memory>
#include <string>

#include "ast.h"

// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

// 定义 parser 函数和错误处理函数的附加参数
// 我们需要返回一个字符串作为 AST, 所以我们把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
// -----------------------------------------------------------
// 返回一个抽象语法树类型作为AST
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数, 也有抽象语法树类型
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
  std::vector<BaseAST*> *list_val;
}

// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT VOID RETURN CONST IF ELSE WHILE BREAK CONTINUE
%token <str_val> IDENT REL_OP EQ_OP LAND LOR
%token <int_val> INT_CONST

// 非终结符的类型定义
%type <ast_val> CompUnits CompUnit FuncDef FuncFParam Block_PRE Block BlockItem Stmt Exp PrimaryExp Number UnaryExp AddExp MulExp RelExp EqExp LAndExp LOrExp Decl ConstDecl BType ConstDef ConstInitVal LVal ConstExp VarDecl VarDef InitVal Matched_Stmt Open_If
%type <str_val> UnaryOp MulOp AddOp 
%type <list_val> FuncFParams ConstDefs VarDefs Exps Dims ConstInitVals InitVals

%%

// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// FuncDef 会返回一个 ast_val
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值


MyCompUnit
  : CompUnits {
    auto comp_unit = (CompUnitAST*)($1);
    ast = unique_ptr<CompUnitAST>(comp_unit);
  };

CompUnits
  : CompUnits CompUnit{
    auto ast = (CompUnitAST*)($1);
    auto another = (CompUnitAST*)($2);
    for (int i = 0; i < another->func_def.size(); ++i)
      ast->func_def.push_back(move(another->func_def[i]));
    for (int i = 0; i < another->decl.size(); ++i)
      ast->decl.push_back(move(another->decl[i]));
    delete another;
    $$ = ast;
  }
  | CompUnit {
    $$ = $1;
  };


CompUnit 
  : FuncDef {
    auto ast = new CompUnitAST();
    ast->decl.clear();
    ast->func_def.clear();
    ast->func_def.push_back(unique_ptr<BaseAST>($1));
    $$ = ast;
  }
  | Decl {
    auto ast = new CompUnitAST();
    ast->decl.clear();
    ast->func_def.clear();
    ast->decl.push_back(unique_ptr<BaseAST>($1));
    $$ = ast;
  };

// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 套用智能指针unique_ptr是为了自动delete，防止内存泄漏
// $index的编号从1开始

FuncDef
  : BType IDENT '(' ')' Block {    
    auto ast = new FuncDefAST();
    auto ftype = new FuncTypeAST($1); delete $1;
    ast->func_type = unique_ptr<BaseAST>((BaseAST*) ftype);
    ast->ident = *unique_ptr<string>($2);
    ast->params.clear();
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | BType IDENT '(' FuncFParams ')' Block {
    auto ast = new FuncDefAST();
    auto ftype = new FuncTypeAST($1); delete $1;
    ast->func_type = unique_ptr<BaseAST>((BaseAST*) ftype);
    ast->ident = *unique_ptr<string>($2);
    ast->params.clear();
    auto list = (std::vector<BaseAST*> *)($4);
    for (auto it = list->begin(); it!= list->end(); ++it)
      ast->params.push_back(unique_ptr<BaseAST>(*it));
    ast->block = unique_ptr<BaseAST>($6);
    $$ = ast;
  };

FuncFParams
  : FuncFParams ',' FuncFParam {
    auto list = (std::vector<BaseAST*> *)($1);
    list->push_back($3);
    $$ = list;
  }
  | FuncFParam {
    auto list = new std::vector<BaseAST*>();
    list->push_back($1);
    $$ = list;
  };

//FuncFParam ::= BType IDENT ["[" "]" {"[" ConstExp "]"}];

FuncFParam  
  : BType IDENT {
    auto ast = new FuncFParamAST();
    ast->btype = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    $$ = ast;
  }
  | BType IDENT '[' ']' {
    auto ast = new FuncFParamAST();
    ast->btype = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->isptr = true;
    $$ = ast;
  }
  | BType IDENT '[' ']' Dims {
    auto ast = new FuncFParamAST();
    ast->btype = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->isptr = true;
    ast->dim_exps.clear();
    auto list = (std::vector<BaseAST*>*)($5);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast->dim_exps.push_back(unique_ptr<BaseAST>(*it));
    delete ($5);
    $$ = ast;
  };

//注意 单个字符一定是用''单引号标识
Block_PRE
  : '{' {
    auto ast = new BlockAST();
    ast->block_item.clear();
    $$ = ast;
  }
  | Block_PRE BlockItem {
    auto ast = (BlockAST*)$1;
    ast->block_item.push_back(unique_ptr<BaseAST>($2));
    $$ = ast;
  };
Block
  : Block_PRE '}' {
    $$ = $1;
  };

BlockItem 
  : Decl {
    $$ = $1;
  }
  | Stmt {
    $$ = $1;
  };

Stmt
  : Matched_Stmt {
    $$ = $1;
  }
  | Open_If {
    $$ = $1;
  };

Open_If
  : IF '(' Exp ')' Stmt {
    auto ast = new StmtAST();
    ast->type = STMT_IF;
    ast->lval = NULL;
    ast->exp = unique_ptr<BaseAST>($3);
    
    //要求then都指向block
    auto then_ptr = (BaseAST*)$5;
    if (then_ptr -> tag == AST_STMT) {
      auto block1 = new BlockAST();
      block1->block_item.clear();
      block1->block_item.push_back(unique_ptr<BaseAST>($5));
      ast->stmt1 = unique_ptr<BaseAST>(block1);
    }
    else //AST_BLOCK
      ast->stmt1 = unique_ptr<BaseAST>($5);
    // assert(then_ptr != NULL);
    // delete then_ptr;
    
    $$ = ast;
  }
  | IF '(' Exp ')' Matched_Stmt ELSE Open_If {
    auto ast = new StmtAST();
    ast->type = STMT_IF;
    ast->lval = NULL;
    ast->exp = unique_ptr<BaseAST>($3);
    
    //要求then和else实际上都指向block
    auto then_ptr = (BaseAST*)$5;
    if (then_ptr -> tag == AST_STMT) {
      auto block1 = new BlockAST();
      block1->block_item.clear();
      block1->block_item.push_back(unique_ptr<BaseAST>($5));
      ast->stmt1 = unique_ptr<BaseAST>(block1);
    }
    else //AST_BLOCK
      ast->stmt1 = unique_ptr<BaseAST>($5);
    // assert(then_ptr != NULL);
    // delete then_ptr;

    auto else_ptr = (BaseAST*)$7;
    if (else_ptr -> tag == AST_STMT) {
      auto block2 = new BlockAST();
      block2->block_item.clear();
      block2->block_item.push_back(unique_ptr<BaseAST>($7));
      ast->stmt2 = unique_ptr<BaseAST>(block2);
    }
    else
      ast->stmt2 = unique_ptr<BaseAST>($7);
    // delete else_ptr;

    $$ = ast;
  }; 

Matched_Stmt
  : IF '(' Exp ')' Matched_Stmt ELSE Matched_Stmt {
    auto ast = new StmtAST();
    ast->type = STMT_IF;
    ast->lval = NULL;
    ast->exp = unique_ptr<BaseAST>($3);
    
    //要求then和else实际上都指向block
    auto then_ptr = (BaseAST*)$5;
    if (then_ptr -> tag == AST_STMT) {
      auto block1 = new BlockAST();
      block1->block_item.clear();
      block1->block_item.push_back(unique_ptr<BaseAST>($5));
      ast->stmt1 = unique_ptr<BaseAST>(block1);
    }
    else //AST_BLOCK
      ast->stmt1 = unique_ptr<BaseAST>($5);
    // assert(then_ptr != NULL);
    // delete then_ptr;

    auto else_ptr = (BaseAST*)$7;
    if (else_ptr -> tag == AST_STMT) {
      auto block2 = new BlockAST();
      block2->block_item.clear();
      block2->block_item.push_back(unique_ptr<BaseAST>($7));
      ast->stmt2 = unique_ptr<BaseAST>(block2);
    }
    else
      ast->stmt2 = unique_ptr<BaseAST>($7);
    // delete else_ptr;

    $$ = ast;
  }
  | LVal '=' Exp ';' {
    auto ast = new StmtAST();
    ast->type = STMT_ASSIGNMENT;
    ast->lval = unique_ptr<BaseAST>($1);
    ast->exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | RETURN Exp ';' {
    auto ast = new StmtAST();
    ast->type = STMT_RETURN;
    ast->lval = NULL;
    ast->exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | RETURN ';' {
    auto ast = new StmtAST();
    ast->type = STMT_RETURN;
    ast->lval = NULL;
    ast->exp = NULL;
    $$ = ast;
  }
  | Exp ';' {
    auto ast = new StmtAST();
    ast->type = STMT_EXECUTE;
    ast->lval = NULL;
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | ';' {
    auto ast = new StmtAST();
    ast->type = STMT_EXECUTE;
    ast->lval = NULL;
    ast->exp = NULL;
    $$ = ast;
  }
  | Block {
    $$ = $1;
  }
  | WHILE '(' Exp ')' Stmt {
    auto ast = new StmtAST();
    ast->type = STMT_WHILE;
    ast->lval = NULL;
    ast->exp = unique_ptr<BaseAST>($3);

    //要求while_body指向block
    auto body_ptr = (BaseAST*)$5;
    if (body_ptr -> tag == AST_STMT) {
      auto block = new BlockAST();
      block->block_item.clear();
      block->block_item.push_back(unique_ptr<BaseAST>($5));
      ast->stmt1 = unique_ptr<BaseAST>(block);
    }
    else //AST_BLOCK
      ast->stmt1 = unique_ptr<BaseAST>($5);

    $$ = ast;
  }
  | BREAK ';' {
    auto ast = new StmtAST();
    ast->type = STMT_BREAK;
    ast->lval = NULL;
    ast->exp = NULL;
    $$ = ast;
  }
  | CONTINUE ';' {
    auto ast = new StmtAST();
    ast->type = STMT_CONTINUE;
    ast->lval = NULL;
    ast->exp = NULL;
    $$ = ast;
  };

Exp
  : LOrExp {
    $$ = $1;
  };

PrimaryExp  
  : '(' Exp ')' {
    $$ = $2;
  }
  | Number {
    $$ = $1;
  }
  | LVal {
    $$ = $1;
  };

//Number的返回类型是ast_val, 但INT_CONST是个int_val
Number
  : INT_CONST {
    auto ast = new NumberAST();
    ast -> int_const = $1;
    $$ = ast;
  }
  ;

Exps
  : Exps ',' Exp {
    auto list = (std::vector<BaseAST*> *)($1);
    list->push_back($3);
    $$ = list;
  }
  | Exp {
    auto list = new std::vector<BaseAST*>();
    list->push_back($1);
    $$ = list;
  };

UnaryExp   
  : IDENT '(' ')' {
    auto ast = new FuncExpAST();
    ast->ident = *unique_ptr<string>($1);
    ast->params.clear();
    $$ = ast;
  }
  | IDENT '(' Exps ')' {
    auto ast = new FuncExpAST();
    ast->ident = *unique_ptr<string>($1);
    ast->params.clear();
    auto list = (std::vector<BaseAST*> *)($3);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast->params.push_back(std::unique_ptr<BaseAST>(*it));
    $$ = ast;
  }
  | PrimaryExp {
    $$ = $1;
  }
  | UnaryOp UnaryExp {
    auto ast = new ExpAST();
    ast -> exp1 = NULL;
    ast -> op = *unique_ptr<string>($1);
    ast -> exp2 = unique_ptr<BaseAST>($2);
    if (ast -> op == "+") $$=$2;
    else $$ = ast;
  };

UnaryOp 
  : '+' {
    $$ = new string("+"); 
  }
  | '-' {
    $$ = new string("-");
  }
  | '!' {
    $$ = new string("!");
  };

MulOp
  :'*' {
    $$ = new string("*");
  }
  |'/' {
    $$ = new string("/");
  }
  |'%'{
    $$ = new string("%");
  };

MulExp 
  : UnaryExp {
    $$ = $1;
  }
  | MulExp MulOp UnaryExp {
    auto ast = new ExpAST();
    ast -> exp1 = unique_ptr<BaseAST>($1);
    ast -> op = *unique_ptr<string>($2);
    ast -> exp2 = unique_ptr<BaseAST>($3);
    $$ = ast;
  };

AddOp
  : '+' {
    $$ = new string("+");
  }
  | '-' {
    $$ = new string("-");
  };

AddExp 
  :MulExp {
    $$ = $1;
  } 
  | AddExp AddOp MulExp {
    auto ast = new ExpAST();
    ast -> exp1 = unique_ptr<BaseAST>($1);
    ast -> op = *unique_ptr<string>($2);
    ast -> exp2 = unique_ptr<BaseAST>($3);
    $$ = ast;
  };


RelExp 
  :AddExp {
    $$ = $1;
  } 
  | RelExp REL_OP AddExp {
    auto ast = new ExpAST();
    ast -> exp1 = unique_ptr<BaseAST>($1);
    ast -> op = *unique_ptr<string>($2);
    ast -> exp2 = unique_ptr<BaseAST>($3);
    $$ = ast;
  };

EqExp       
  :RelExp {
    $$ = $1;
  }
  | EqExp EQ_OP RelExp{
    auto ast = new ExpAST();
    ast -> exp1 = unique_ptr<BaseAST>($1);
    ast -> op = *unique_ptr<string>($2);
    ast -> exp2 = unique_ptr<BaseAST>($3);
    $$ = ast;
  };

LAndExp 
  :EqExp {
    $$ = $1;
  } 
  | LAndExp LAND EqExp {
    auto ast = new ExpAST();
    ast -> exp1 = unique_ptr<BaseAST>($1);
    ast -> op = *unique_ptr<string>($2);
    ast -> exp2 = unique_ptr<BaseAST>($3);
    $$ = ast;
  };

LOrExp 
  :LAndExp {
    $$ = $1;
  }
  | LOrExp LOR LAndExp {
    auto ast = new ExpAST();
    ast -> exp1 = unique_ptr<BaseAST>($1);
    ast -> op = *unique_ptr<string>($2);
    ast -> exp2 = unique_ptr<BaseAST>($3);
    $$ = ast;
  };

Decl
  : ConstDecl {
    $$ = $1;
  }
  | VarDecl {
    $$ = $1;
  };

ConstDefs
  : ConstDefs ',' ConstDef {
    auto list = (std::vector<BaseAST*>*)($1);
    list->push_back($3);
    $$ = list;
  }
  | ConstDef {
    auto list = new std::vector<BaseAST*>();
    list->push_back($1);
    $$ = list;
  }

ConstDecl
  : CONST BType ConstDefs ';' {
    auto ast = new DeclAST();
    ast -> type = CONST_DEF;
    ast -> btype = unique_ptr<BaseAST>($2);
    ast -> def.clear();
    auto list = (std::vector<BaseAST*>*)($3);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast -> def.push_back(unique_ptr<BaseAST>(*it));
    $$ = ast;
  };     

VarDecl 
  : BType VarDefs ';' {
    auto ast = new DeclAST();
    ast -> type = VAR_DEF;
    ast -> btype = unique_ptr<BaseAST>($1);
    ast -> def.clear();
    auto list = (std::vector<BaseAST*> *)($2);
    for (auto it = list->begin(); it != list->end(); ++it) 
      ast -> def.push_back(unique_ptr<BaseAST>(*it));
    $$ = ast;
  };

VarDefs
  : VarDefs ',' VarDef {
    auto list = (std::vector<BaseAST*>*)($1);
    list->push_back($3);
    $$ = list;
  }
  | VarDef {
    auto list = new std::vector<BaseAST*>();
    list->clear();
    list->push_back($1);
    $$ = list;
  }; 

BType
  : INT {
    auto ast = new BTypeAST();
    ast->type = BTYPE_INT;
    $$ = ast;
  }
  | VOID {
    auto ast = new BTypeAST();
    ast->type = BTYPE_VOID;
    $$ = ast;
  };

Dims
  : '[' Exp ']' {
    auto list = new std::vector<BaseAST*> ();
    list->clear();
    list->push_back($2);
    $$ = list;
  }
  | Dims '[' Exp ']' {
    auto list = (std::vector<BaseAST*>*)($1);
    list->push_back($3);
    $$ = list;
  };
  
ConstDef      
  : IDENT '=' ConstInitVal {
    auto ast = new DefAST();
    ast -> ident = *std::unique_ptr<string>($1);
    ast -> value = std::unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | IDENT Dims '=' ConstInitVal {
    auto ast = new ArrayDefAST();
    ast -> ident = *std::unique_ptr<string>($1);
    ast -> dim_exps.clear();
    auto list = (std::vector<BaseAST*>*)($2);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast -> dim_exps.push_back(unique_ptr<BaseAST>(*it));
    ast -> value = std::unique_ptr<BaseAST>($4);
    $$ = ast;
  };

ConstInitVal  
  : ConstExp {
    $$ = $1;
  }
  | '{' '}' {
    auto ast = new AggregateAST();
    ast->elements.clear();
    $$ = ast;
  }
  | '{' ConstInitVals '}' {
    auto ast = new AggregateAST();
    ast->elements.clear();
    auto list = (std::vector<BaseAST*>*)($2);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast->elements.push_back(unique_ptr<BaseAST>(*it));
    $$ = ast;
  };

ConstInitVals
  : ConstInitVal {
    auto list = new std::vector<BaseAST*> ();
    list->clear();
    list->push_back($1);
    $$ = list;
  }
  | ConstInitVals ',' ConstInitVal {
    auto list = (std::vector<BaseAST*>*)($1);
    list->push_back($3);
    $$ = list;
  };

VarDef 
  : IDENT {
    auto ast = new DefAST();
    ast -> ident = *std::unique_ptr<string>($1);
    ast -> value = NULL;
    $$ = ast;
  } 
  | IDENT Dims {
    auto ast = new ArrayDefAST();
    ast -> ident = *std::unique_ptr<string>($1);
    ast -> dim_exps.clear();
    auto list = (std::vector<BaseAST*>*)($2);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast -> dim_exps.push_back(unique_ptr<BaseAST>(*it));
    ast -> value = NULL;
    $$ = ast;
  }
  | IDENT '=' InitVal {
    auto ast = new DefAST();
    ast -> ident = *std::unique_ptr<string>($1);
    ast -> value = std::unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | IDENT Dims '=' InitVal {
    auto ast = new ArrayDefAST();
    ast -> ident = *std::unique_ptr<string>($1);
    ast -> dim_exps.clear();
    auto list = (std::vector<BaseAST*>*)($2);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast -> dim_exps.push_back(unique_ptr<BaseAST>(*it));
    ast -> value = std::unique_ptr<BaseAST>($4);
    delete ($2);
    $$ = ast;
  };

LVal 
  : IDENT {
    auto ast = new LValAST();
    ast -> ident = *std::unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT Dims {
    auto ast = new ArrayAST();
    ast -> ident = *std::unique_ptr<string>($1);
    ast -> dim_exps.clear();
    auto list = (std::vector<BaseAST*>*)($2);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast -> dim_exps.push_back(unique_ptr<BaseAST>(*it));
    delete ($2);
    $$ = ast;
  };

ConstExp
 : Exp {
    $$ = $1;
  };

InitVal 
  : Exp {
    $$ = $1;
  }
  | '{' '}' {
    auto ast = new AggregateAST();
    ast->elements.clear();
    $$ = ast;
  }
  | '{' InitVals '}' {
    auto ast = new AggregateAST();
    ast->elements.clear();
    auto list = (std::vector<BaseAST*>*)($2);
    for (auto it = list->begin(); it != list->end(); ++it)
      ast->elements.push_back(unique_ptr<BaseAST>(*it));
    $$ = ast;
  };

InitVals
  : InitVal {
    auto list = new std::vector<BaseAST*> ();
    list->clear();
    list->push_back($1);
    $$ = list;
  }
  | InitVals ',' InitVal {
    auto list = (std::vector<BaseAST*>*)($1);
    list->push_back($3);
    $$ = list;
  };


%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "error: " << s << endl;
}
