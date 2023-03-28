#ifndef AST_H
#define AST_H

#include <iostream>
#include <memory>
#include <assert.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
/*
CompUnit    ::= [CompUnit] (Decl | FuncDef);

Decl          ::= ConstDecl | VarDecl;
ConstDecl     ::= "const" BType ConstDef {"," ConstDef} ";";
BType         ::= "int";
ConstDef      ::= IDENT "=" ConstInitVal;
ConstInitVal  ::= ConstExp;
VarDecl       ::= BType VarDef {"," VarDef} ";";
VarDef        ::= IDENT | IDENT "=" InitVal;
InitVal       ::= Exp;

FuncDef     ::= FuncType IDENT "(" [FuncFParams] ")" Block;
FuncType    ::= "void" | "int";
FuncFParams ::= FuncFParam {"," FuncFParam};
FuncFParam  ::= BType IDENT;

Block         ::= "{" {BlockItem} "}";
BlockItem     ::= Decl | Stmt;
Stmt          ::= LVal "=" Exp ";"
                | [Exp] ";"
                | Block
                |  "if" "(" Exp ")" Stmt ["else" Stmt]
                | "return" [Exp] ";"
                | "while" "(" Exp ")" Stmt
                | "break" ";"
                | "continue" ";" ;

Exp         ::= AddExp;
LVal          ::= IDENT;
PrimaryExp    ::= "(" Exp ")" | LVal | Number;
Number      ::= INT_CONST;
FuncRParams ::= Exp {"," Exp};
UnaryExp    ::= IDENT "(" [FuncRParams] ")" | PrimaryExp | UnaryOp UnaryExp;
UnaryOp     ::= "+" | "-" | "!";
MulExp      ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
AddExp      ::= MulExp | AddExp ("+" | "-") MulExp;
RelExp        ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
EqExp         ::= RelExp | EqExp ("==" | "!=") RelExp;
LAndExp       ::= EqExp | LAndExp "&&" EqExp;
LOrExp        ::= LAndExp | LOrExp "||" LAndExp;
ConstExp      ::= Exp;

-----------------------------------------
ConstDef      ::= IDENT ["[" ConstExp "]"] "=" ConstInitVal;
ConstInitVal  ::= ConstExp | "{" [ConstExp {"," ConstExp}] "}";
VarDef        ::= IDENT ["[" ConstExp "]"]
                | IDENT ["[" ConstExp "]"] "=" InitVal;
InitVal       ::= Exp | "{" [Exp {"," Exp}] "}";

LVal          ::= IDENT ["[" Exp "]"];

FuncFParam ::= BType IDENT ["[" "]" {"[" ConstExp "]"}];

*/

typedef std::vector<int>* dims_t;

typedef enum {
  DATA_UNKNOWN, DATA_INT, DATA_ARRAY, DATA_PTR, 
}data_tag_t;
struct data_t{
  data_tag_t tag;
  union {
    int number;
    dims_t dims; //如果是数组的话，存维度信息的指针
  }value;
  data_t() { 
    tag = DATA_UNKNOWN; value.number = 0;
  }
  data_t(data_tag_t _tag, int _value) {
    tag = _tag; value.number = _value;
  }
};

typedef enum {
  FUNCTYPE_VOID, 
  FUNCTYPE_INT, 
  FUNCTYPE_INVALID, 
}functype_t;
struct func_info_t {
  functype_t type;
  int arg_num;
  func_info_t () {
    type = FUNCTYPE_INVALID, arg_num = -1;
  }
  func_info_t (functype_t _type, int _num) {
    type = _type, arg_num = _num;
  }
};

class Koopa_Symbol_Table_t {
 public:
  int scope_num;
  class scope_symbol_table_t { //为了维护符号表，建立链状作用域符号表类
    public:
      int index;
      std::unordered_set <std::string>* ident_table; //常量，变量符号表 考虑常量数组和变量数组
      std::unordered_map <std::string, data_t>* const_symbol_table; //常数符号表
      std::unordered_map <std::string, data_t>* array_symbol_table; //数组符号表
      scope_symbol_table_t* father_scope;
      //规定定义在scpore idx的符号ident会写作ident_idx
    scope_symbol_table_t() {
      father_scope = NULL;
      ident_table = new std::unordered_set <std::string>(); ident_table->clear();
      const_symbol_table = new std::unordered_map <std::string, data_t>(); const_symbol_table->clear();
      array_symbol_table = new std::unordered_map <std::string, data_t>(); array_symbol_table->clear();
    }
    ~scope_symbol_table_t() {
      delete ident_table;
      delete const_symbol_table;
      delete array_symbol_table;
    }
  };
  std::unordered_map <std::string, func_info_t>* func_table; //函数表，只要全局一个
  scope_symbol_table_t* current_scope;
  
  Koopa_Symbol_Table_t() {
    current_scope = new scope_symbol_table_t(); //初始化全局作用域
    current_scope->index = scope_num = 0;

    func_table = new std::unordered_map <std::string, func_info_t>(); 
    func_table->clear(); //初始化全局函数表
  }
  ~Koopa_Symbol_Table_t() {
    while (current_scope != NULL) {
      auto next_scope = current_scope->father_scope;
      delete current_scope;
      current_scope = next_scope;
    }
    delete func_table;
  }

  int getScopeIndex() {
    return ++scope_num;
  }
  void addScope() {
    auto ptr = new scope_symbol_table_t();
    ptr->index = ++scope_num;
    ptr->father_scope = current_scope;
    current_scope = ptr;
  }
  void removeScope() {
    auto ptr = current_scope;
    current_scope = current_scope->father_scope;
    delete ptr;
  }
  bool isOKtoDef(const std::string& ident) { //查询ident是否在当前scope有声明
    auto itable = current_scope->ident_table;
    return itable->find(ident) == itable->end();
  }
  std::string questIdent(const std::string& ident) { //查询ident是否有声明，如果有，是变量还是常量？
    //如果ident不存在，返回空串； 如果是变量(或者函数; 再或者数组)，返回"@ident_idx"；如果是常量，返回string(常量值)
    auto ptr = current_scope;
    while (ptr != NULL) {
      auto itable = ptr->ident_table;
      if (itable->find(ident) != itable->end()) { //找到声明
        auto ctable = ptr->const_symbol_table;
        auto it = ctable->find(ident);
        if (it != ctable->end()) { //常量
          data_t const_value = it -> second;
          return std::to_string(const_value.value.number);
        }
        return "@" + ident + "_" + std::to_string(ptr->index);
      }
      ptr = ptr->father_scope;
    }
    return "";
  }
  data_t questConstant(const std::string& ident) { //查询ident是否为常量
    //如果ident不存在或者是变量(或者函数)，返回data_t(DATA_UNKNOWN, 0)；如果是常量，返回data_t
    auto ptr = current_scope;
    while (ptr != NULL) {
      auto itable = ptr->ident_table;
      if (itable->find(ident) != itable->end()) { //找到声明
        auto ctable = ptr->const_symbol_table;
        auto it = ctable->find(ident);
        if (it != ctable->end()) { //常量
          return it -> second;
        }
        return data_t(DATA_UNKNOWN, 0);
      }
      ptr = ptr->father_scope;
    }
    return data_t(DATA_UNKNOWN, 0);
  }
  data_t questArray(const std::string& ident) { //查询ident是否为数组或数组指针
    //如果ident不存在或者不是数组，返回data_t(DATA_UNKOWN, 0); 如果是数组，返回数组信息data_t
    auto ptr = current_scope;
    while (ptr != NULL) {
      auto itable = ptr->ident_table;
      if (itable->find(ident) != itable->end()) { //找到声明
        auto atable = ptr->array_symbol_table;
        auto it = atable->find(ident);
        if (it != atable->end()) { //数组或数组指针
          return it -> second;
        }
        return data_t(DATA_UNKNOWN, 0);
      }
      ptr = ptr->father_scope;
    }
    return data_t(DATA_UNKNOWN, 0);
  }
  func_info_t questFunc(const std::string &ident) {//查询名为ident的函数
    //如果ident不存在或者不是函数，返回func_info_t(FUNCTYPE_INVALID, -1)；如果是函数，返回具体信息
    auto ptr = current_scope;
    while (ptr != NULL) {
      auto itable = ptr->ident_table;
      if (itable->find(ident) != itable->end()) { //找到同名的变量/常量
        return func_info_t();
      }
      ptr = ptr->father_scope;
    }

    //如果ident不是变量名，检查有没有这个函数名
    auto it = func_table->find(ident);
    if (it != func_table->end()) { //查到这个函数，返回函数信息
      return it -> second;
    }

    //否则没有这个函数
    return func_info_t();
  }
  std::string addIdent(const std::string& ident) { //在ident表里加
    auto itable = current_scope->ident_table;
    itable->insert(ident);
    return "@" + ident + "_" + std::to_string(current_scope->index);
  }
  void addConstant(const std::string& ident, const data_t& value) { //在常数表里加
    auto ctable = current_scope->const_symbol_table;
    ctable->insert(make_pair(ident, value));
    return;
  }
  void addArray(const std::string& ident, const data_t& value) { //在数组符号表里加
    auto atable = current_scope->array_symbol_table;
    atable->insert(make_pair(ident, value));
    return;
  }
  void addFunc(const std::string& ident, const func_info_t& info) { //在函数表里加一项
    func_table->insert(make_pair(ident, info));
    return;
  }
};

extern int KoopaReg_id;
extern int value_i;
extern Koopa_Symbol_Table_t Koopa_Symbol_Table;

typedef enum {
  AST_COMPUNIT, 
  AST_FUNCFPARAM, 
  AST_FUNCDEF,
  AST_FUNCTYPE,
  AST_BLOCK,
  AST_BLOCKITEM, 
  AST_STMT,
  AST_EXP,
  AST_NUMBER, 
  AST_DECL, 
  AST_BTYPE, 
  AST_DEF, 
  AST_LVAL,
  AST_FUNCEXP, 
  AST_ARRAYDEF, 
  AST_AGGREGATE, 
  AST_ARRAY,
}AST_tag_t;

typedef enum {
  CONST_DEF, 
  VAR_DEF, 
  GLOBAL_CONST_DEF, 
  GLOBAL_VAR_DEF,
}decl_type_t;

typedef enum {
  STMT_ASSIGNMENT, 
  STMT_RETURN, 
  STMT_EXECUTE, 
  STMT_IF, 
  STMT_WHILE, 
  STMT_BREAK, 
  STMT_CONTINUE, 
}Stmt_type_t;

typedef enum {
  EXP_CONDITION, 
  EXP_EVALUATION,
}exp_type_t;

typedef enum {
  BTYPE_INT,
  BTYPE_VOID, 
  BTYPE_INVALID, 
}btype_type_t;

// 所有 AST 的基类
class BaseAST {
 public:
  AST_tag_t tag;
  BaseAST(AST_tag_t tag): tag(tag) {}
  virtual ~BaseAST() = default;
  // virtual void Dump() const = 0;
  virtual std::string IR() const = 0;
  virtual data_t calculate() const {
    return data_t(DATA_UNKNOWN, 0);
  }
  virtual data_t aggregate_calculate() {
    return data_t(DATA_UNKNOWN, 0);
  }
  
  virtual void begin_new_scope() const {}
  virtual void end_new_scope() const {}
  virtual void set_break_label(const std::string& label) {}
  virtual void set_continue_label(const std::string& label) {}
  virtual void alloc_for_Fparam(std::string &contex) const {}

  virtual functype_t get_functype() const { 
    return FUNCTYPE_INVALID; 
  }
  virtual func_info_t get_funcExp_info() const {
    return func_info_t();
  }

  virtual void set_decl_global() {}
  virtual void set_def_type(decl_type_t _type) {}
  virtual void set_def_firstDef() {}
  virtual void set_def_btype_str(const std::string& str) {}
  virtual void get_Fparam_ptr_type() {}
  virtual void get_def_array_type() {}

  virtual void set_block_entryname(const std::string& name) {}
  virtual void set_block_contex(const std::string& contex) {}
  virtual void set_block_creatingScopeExternally() {}

  virtual void set_array_dims(const dims_t& _dims) {}

  virtual void set_exp_type(exp_type_t _type) {}
  virtual void set_exp_true_label(const std::string& label) {}
  virtual void set_exp_false_label(const std::string& label) {}

  virtual btype_type_t get_btype_type() const {
    return BTYPE_INVALID;
  }
};
std::string get_Koopa_reg_ir(const std::unique_ptr<BaseAST>& exp, std::string& contex);
void get_array_init(const dims_t& dims, const dims_t& value, int dim_i, const std::string& ir, std::string& contex); 

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
 public:
  std::vector<std::unique_ptr<BaseAST>> decl;
  std::vector<std::unique_ptr<BaseAST>> func_def;

  CompUnitAST(): BaseAST(AST_COMPUNIT) {}
  // void Dump() const override {
  //   func_def->Dump();
  // }
  std::string libFunc_declare() const {
    std::string contex = "decl @getint(): i32\n";
    contex = contex + "decl @getch(): i32\n";
    contex = contex + "decl @getarray(*i32): i32\n";
    contex = contex + "decl @putint(i32)\n";
    contex = contex + "decl @putch(i32)\n";
    contex = contex + "decl @putarray(i32, *i32)\n";
    contex = contex + "decl @starttime()\n";
    contex = contex + "decl @stoptime()\n";
    //注册库函数
    Koopa_Symbol_Table.addFunc("getint", func_info_t(FUNCTYPE_INT, 0)); 
    Koopa_Symbol_Table.addFunc("getch", func_info_t(FUNCTYPE_INT, 0)); 
    Koopa_Symbol_Table.addFunc("getarray", func_info_t(FUNCTYPE_INT, 1));
    Koopa_Symbol_Table.addFunc("putint", func_info_t(FUNCTYPE_VOID, 1)); 
    Koopa_Symbol_Table.addFunc("putch", func_info_t(FUNCTYPE_VOID, 1)); 
    Koopa_Symbol_Table.addFunc("putarray", func_info_t(FUNCTYPE_VOID, 2)); 
    Koopa_Symbol_Table.addFunc("starttime", func_info_t(FUNCTYPE_VOID, 0)); 
    Koopa_Symbol_Table.addFunc("stoptime", func_info_t(FUNCTYPE_VOID, 0)); 
    return contex;
  }
  std::string IR () const override {
    std::string ir = libFunc_declare();
    for (int i = 0; i < decl.size(); ++i) { //全局变量/常量注册或声明
      decl[i]->set_decl_global();
      ir = ir + decl[i]->IR();
    }
    for (int i = 0; i < func_def.size(); ++i) 
      ir = ir + func_def[i]->IR();
    return ir;
  }
};

//FuncFParam
class FuncFParamAST : public BaseAST {
 public:
  std::string ident;
  std::unique_ptr<BaseAST> btype;

  bool isptr;
  std::vector<std::unique_ptr<BaseAST>> dim_exps;
  dims_t ptr_type;
  std::string ptr_type_str;

  FuncFParamAST() : BaseAST(AST_FUNCFPARAM) {
    isptr = false;
    dim_exps.clear();
    ptr_type = NULL;
    ptr_type_str = "";
  }
  ~FuncFParamAST() {
    if (ptr_type != NULL) delete ptr_type;
  }

  void get_Fparam_ptr_type() override { //在形参定义时，确定指针类形参的维数和各个维度width
    if (!isptr) return;
    ptr_type = new std::vector<int> ();
    ptr_type->clear(); ptr_type->push_back(0);
    for (int i = 0, sze = dim_exps.size(); i < sze; ++i) {
      data_t width = dim_exps[i]->calculate();
      assert(width.tag == DATA_INT);
      ptr_type->push_back(width.value.number);
    }

    std::string ir = btype -> IR();
    for (int i = ptr_type->size() - 1; i >= 1; --i) {
      ir = "[" + ir + ", " + std::to_string((*ptr_type)[i]) + "]";
    }
    ptr_type_str = "*" + ir;

    return;
  }

  std::string IR() const override {
    //形参声明，确保没有形参重名
    assert(Koopa_Symbol_Table.isOKtoDef(ident));
    if (!isptr) {//形参是个变量
      std::string name = "";
      name = Koopa_Symbol_Table.addIdent(ident);
      name[0] = '%'; //规定形参形如 %name
      std::string ir = name + ": " + btype->IR();
      return ir;
    }
    else { //形参是个数组指针
      data_t ptr_data;
      ptr_data.tag = DATA_PTR;
      ptr_data.value.dims = ptr_type;
      Koopa_Symbol_Table.addArray(ident, ptr_data);

      std::string name = Koopa_Symbol_Table.addIdent(ident);
      name[0] = '%';
      std::string ir = name + ": " + ptr_type_str;
      return ir;
    }
  }
  void alloc_for_Fparam(std::string &contex) const override {
    if (!isptr) {
      std::string name = Koopa_Symbol_Table.questIdent(ident);
      std::string Fname = name; Fname[0] = '%';
      contex = contex + "  " + name + " = alloc " + btype->IR() + "\n";
      contex = contex + "  store " + Fname + ", " + name + "\n";
    }
    else {
      std::string name = Koopa_Symbol_Table.questIdent(ident);
      std::string Fname = name; Fname[0] = '%';
      contex = contex + "  " + name + " = alloc " + ptr_type_str + "\n";
      contex = contex + "  store " + Fname + ", " + name + "\n";
    }
    return;
  }
};

// FuncDef 也是 BaseAST
static int retBlock_num = 0;
class FuncDefAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> func_type;
  std::string ident;
  std::vector <std::unique_ptr<BaseAST> > params;
  std::unique_ptr<BaseAST> block;

  /*
  具名符号@name和临时符号%name没有任何区别
  为了形式同一，规定函数形参写成 %x, 到函数body中会专门alloc一个@x来装它
  例如，
    int half(int x) {
      return x / 2;
    }
  将被我翻译成
    fun @half(%x: i32): i32 {
    %entry:
      @x = alloc i32
      store %x, @x
      %0 = load @x
      %1 = div %0, 2
      ret %1
    }
  */
  FuncDefAST(): BaseAST(AST_FUNCDEF){}
  std::string IR() const override {
    //std::cerr << "parsing function " << ident << std::endl;
    functype_t mytype = func_type->get_functype();
    Koopa_Symbol_Table.addFunc(ident, func_info_t(mytype, params.size())); //注册到函数表里

    block -> set_block_entryname("%entry");
    block->set_block_creatingScopeExternally();
    block->begin_new_scope();

    std::string alloc_Fparam_contex = "";
    std::string ir = "fun @" + ident + "(";
    for (int i = 0; i < params.size(); ++i) {
      params[i] -> get_Fparam_ptr_type();

      if (i != 0) ir = ir + ", ";
      ir = ir + params[i]->IR();
      params[i]->alloc_for_Fparam(alloc_Fparam_contex);
    }
    ir = ir + ")" + func_type->IR() + " {\n";
    block->set_block_contex(alloc_Fparam_contex);
    ir = ir + block->IR();
    block->end_new_scope();
    if (mytype == FUNCTYPE_INT) ir = ir + "  ret 0\n";
    else ir = ir + "  ret\n";
    ir = ir + "}\n";
    return ir;
  }
};

class FuncTypeAST : public BaseAST { 
  public:
    // void Dump() const override {
    //   std::cout << "i32";
    // }
    functype_t type;
    FuncTypeAST(): BaseAST(AST_FUNCTYPE) {}
    FuncTypeAST(BaseAST* ast): BaseAST(AST_FUNCTYPE) {
      assert(ast->tag == AST_BTYPE);  
      btype_type_t btype = ast->get_btype_type();
      assert(btype != BTYPE_INVALID);
      switch (btype)
      {
      case BTYPE_INT:
        type = FUNCTYPE_INT;
        break;
      case BTYPE_VOID:
        type = FUNCTYPE_VOID;
        break;
      default:
        assert(false);
        break;
      }
    }
    functype_t get_functype()const override {
      return type;
    }
    std::string IR() const override {
      if (type == FUNCTYPE_INT) return std::string(": i32");
      return "";
    }
};

class BlockAST : public BaseAST {
  public:
    std::string entryname;
    std::string block_contex;
    std::string break_label;
    std::string continue_label;
    std::vector <std::unique_ptr<BaseAST> > block_item; 
    bool creatingScopeExternally;
  
    // void Dump() const override {
    //   std::cout << "%entry: \n";
    //   stmt->Dump();
    // }
    BlockAST(): BaseAST(AST_BLOCK){
      entryname = "";
      block_contex = "";
      break_label = "";
      continue_label = "";
      creatingScopeExternally = false;
    }
    
    void set_break_label(const std::string& label) override {
      break_label = label;
    }
    void set_continue_label(const std::string& label) override {
      continue_label = label;
    }
    void set_block_entryname(const std::string& name) override {
      entryname = name;
    }
    void set_block_contex(const std::string& contex) override {
      block_contex = contex;
    }
    void set_block_creatingScopeExternally() override {
      creatingScopeExternally = true;
    }
    void begin_new_scope() const override {
      Koopa_Symbol_Table.addScope();
    }
    void end_new_scope() const override {
      Koopa_Symbol_Table.removeScope();
    }
    std::string IR() const override { 
      //std::cerr << "parsing block " << std::endl;
      if (!creatingScopeExternally) begin_new_scope();
      std::string ir = (entryname != "") ? entryname + ":\n" : "";
      ir = ir + block_contex;
      if (break_label != "") {
        for (int i = 0; i < block_item.size(); ++i) {
          block_item[i]->set_break_label(break_label);
          block_item[i]->set_continue_label(continue_label);
        }
      }
      for (int i = 0; i < block_item.size(); ++i) 
        ir = ir + block_item[i]->IR();
      if (!creatingScopeExternally) end_new_scope();
      return ir;
    }  
};

// class BlockItemAST : public BaseAST {
//   public:
//     std::unique_ptr<BaseAST> item;

//   BlockItemAST(): BaseAST(AST_BLOCKITEM){}
//   std::string IR() const override {
//     std::string contex = "";
//     if (isAfterRet) {
//       contex = "%new_block_label_" + std::to_string(++retBlock_num) + ":\n";
//       isAfterRet = false;
//     }
//     std::string ir = item -> IR();
//     return contex + ir;
//   }  
// };

class DeclAST : public BaseAST {
  public:
    decl_type_t type;
    std::unique_ptr<BaseAST> btype;
    std::vector <std::unique_ptr<BaseAST> > def;
    DeclAST(): BaseAST(AST_DECL){}
    void set_decl_global() override {
      if (type == CONST_DEF) type = GLOBAL_CONST_DEF;
      if (type == VAR_DEF) type = GLOBAL_VAR_DEF;
    }
    std::string IR() const override {
      std::string ir = "";
      std::string btype_str = btype -> IR();
      for (int i = 0; i < def.size(); ++i) {
        def[i] -> set_def_type(type); //下传def类型标签
        def[i] -> set_def_firstDef(); //在声明中的标签应当是首次定义的
        def[i] -> set_def_btype_str(btype_str);
        def[i] -> get_def_array_type();
        // DefAST* defi = (DefAST*)def[i];
        // defi -> type = type; 
        // defi -> firstDef = true; 
        ir = ir + def[i] -> IR();
      }
      return ir; 
    }
};
class BTypeAST : public BaseAST {
  public:
    btype_type_t type;
    BTypeAST(): BaseAST(AST_BTYPE){
      type = BTYPE_INT;
    }
    btype_type_t get_btype_type() const override {
      return type;
    }
    std::string IR() const override {
      if (type == BTYPE_INT) return std::string("i32");
      return "";
    }
};

class ArrayDefAST : public BaseAST {
  public:
    decl_type_t type;
    std::string ident;
    std::string btype_str;
    std::vector<std::unique_ptr<BaseAST>> dim_exps;
    std::unique_ptr<BaseAST> value;

    bool firstDef;
    std::string array_type_str;
    dims_t dims;

    ArrayDefAST(): BaseAST(AST_ARRAYDEF) {
      firstDef = false;
      btype_str = "";
      dims = NULL;
    }
    ~ArrayDefAST() {
      if (dims != NULL) delete dims; 
    }
    void set_def_type(decl_type_t _type) override {
      type = _type;
    }
    void set_def_firstDef() override {
      firstDef = true;
    }
    void set_def_btype_str(const std::string& str) override {
      btype_str = str;
    }
    void get_def_array_type() override { //在数组被声明/定义时出现，确定数组的维数和各个维度width
      dims = new std::vector<int> ();
      dims->clear();
      for (int i = 0, sze = dim_exps.size(); i < sze; ++i) {
        data_t width = dim_exps[i]->calculate();
        assert(width.tag == DATA_INT);
        dims->push_back(width.value.number);
      }

      std::string ir = btype_str;
      for (int i = dims->size() - 1; i >= 0; --i) {
        ir = "[" + ir + ", " + std::to_string((*dims)[i]) + "]";
      }
      array_type_str = ir;

      return;
    }
    std::string IR() const override {
      std::string contex = "";
      std::string keyIR = "";

      if (firstDef) { //不可以重名
        assert(Koopa_Symbol_Table.isOKtoDef(ident));
      }
      else { //非声明时，要求该数组已经声明
        assert(Koopa_Symbol_Table.questIdent(ident) != "");
        assert(Koopa_Symbol_Table.questArray(ident).tag != DATA_UNKNOWN);
      }

      if (type == GLOBAL_CONST_DEF || type == GLOBAL_VAR_DEF) { //全局数组 一定是firstDef
          data_t array_data = data_t();
          array_data.tag = DATA_ARRAY;
          array_data.value.dims = dims;

          std::string name = Koopa_Symbol_Table.addIdent(ident);
          Koopa_Symbol_Table.addArray(ident, array_data);
          keyIR = "global " + name + " = alloc " + array_type_str;
          if (type == GLOBAL_CONST_DEF) assert(value != NULL); //常数数组必须要有value
          if (value == NULL) keyIR = keyIR + ", zeroinit\n";
          else {
            value->set_array_dims(dims);
            value->aggregate_calculate();
            keyIR = keyIR + ", " + value->IR() + "\n";
          }
      }
      else if (type == CONST_DEF || type == VAR_DEF) {
        std::string name;
        data_t array_data = data_t();
        if (firstDef) {
          array_data.tag = DATA_ARRAY;
          array_data.value.dims = dims;
          
          name = Koopa_Symbol_Table.addIdent(ident);
          Koopa_Symbol_Table.addArray(ident, array_data);
          contex = "  " + name + " = alloc " + array_type_str + "\n";
        }
        else {
          array_data = Koopa_Symbol_Table.questArray(ident);
          assert(array_data.tag == DATA_ARRAY);

          name = Koopa_Symbol_Table.questIdent(ident);
        }

        if (type == CONST_DEF) assert(value != NULL);
        if (value != NULL) {
          value->set_array_dims(dims);
          data_t value_data = value->aggregate_calculate();
          assert(value_data.tag == DATA_ARRAY);

          value_i = 0;
          get_array_init(array_data.value.dims, value_data.value.dims, 0, name, keyIR);
        }
      }
      return contex + keyIR;
    }
};

class DefAST : public BaseAST {
  public:
    decl_type_t type;
    bool firstDef;
    std::string ident;
    std::string btype_str;
    std::unique_ptr<BaseAST> value;
    DefAST(): BaseAST(AST_DEF) {
      firstDef = false;
      btype_str = "";
    }
    void set_def_type(decl_type_t _type) override {
      type = _type;
    }
    void set_def_firstDef() override {
      firstDef = true;
    }
    void set_def_btype_str(const std::string& str) override {
      btype_str = str;
    }
    std::string IR() const override {
      std::string contex = "";
      std::string keyIR = "";

      if (firstDef) { //不可以重名
        assert(Koopa_Symbol_Table.isOKtoDef(ident));
      }
      else { //非声明时，要求该变量已经声明
        assert(Koopa_Symbol_Table.questIdent(ident) != "");
      }

      if (type == CONST_DEF || type == GLOBAL_CONST_DEF) { //常量定义, 不会有专门ir表示
        assert(value != NULL);
        data_t rhs = value -> calculate();
        assert(rhs.tag != DATA_UNKNOWN); //要求常量值已知
        Koopa_Symbol_Table.addConstant(ident, rhs);
        Koopa_Symbol_Table.addIdent(ident); //常量定义一定是firstDef
      }
      else if (type == VAR_DEF) { //变量
        std::string name = "";
        if (firstDef) {
          name = Koopa_Symbol_Table.addIdent(ident);
          contex = contex + "  " + name + " = alloc " + btype_str +"\n";
        }
        else name = Koopa_Symbol_Table.questIdent(ident);
        assert(name[0] == '@');

        if (value != NULL) {
          std::string ir = get_Koopa_reg_ir(value, contex);
          keyIR = "  store " + ir + ", " + name + "\n";
        }
      }
      else if (type == GLOBAL_VAR_DEF) { //全局变量
        std::string name = Koopa_Symbol_Table.addIdent(ident);
        keyIR = "global " + name + " = alloc " + btype_str;
        if (value == NULL) keyIR = keyIR + ", zeroinit\n";
        else {
          //TBD 不确定全局变量的初值会不会是一个运行时计算表达式
          data_t rhs = value -> calculate();
          assert(rhs.tag != DATA_UNKNOWN); //要求全局变量初值已知
          keyIR = keyIR + ", " + std::to_string(rhs.value.number) + "\n";
        }
      }
      return contex + keyIR;
    }
};

static int IfStmt_num = -1;
static int WhileStmt_num = -1;
static int BreakStmt_num = -1;
static int ContinueStmt_num = -1;
class StmtAST : public BaseAST {
  public:
    Stmt_type_t type;
    std::unique_ptr<BaseAST> lval;
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> stmt1;
    std::unique_ptr<BaseAST> stmt2;
    std::string break_label;
    std::string continue_label;
    
    // void Dump() const override {
    //   std::cout << "  ret " << number << "\n";
    // }  
    StmtAST(): BaseAST(AST_STMT){
      stmt1 = NULL; stmt2 = NULL;
      break_label = ""; continue_label = "";
    }
    void set_break_label(const std::string& label) override {
      break_label = label;
    }
    void set_continue_label(const std::string& label) override {
      continue_label = label;
    }
    std::string IR() const override {
      std::string contex = "";
      std::string ir = ""; 
                //ir = (exp == NULL) ? "" : get_Koopa_reg_ir(exp, contex); 
      std::string keyIR = "";

      if (type == STMT_RETURN) { //在return之后要新开一个代码块
        //std::cerr << "parsing stmt RETURN\n";
        if (exp != NULL) ir = get_Koopa_reg_ir(exp, contex); 
        contex = contex + "  ret " + ir + "\n";
        keyIR = "%new_block_label_" + std::to_string(++retBlock_num) + ":\n";
        return contex + keyIR;
      }
      if (type == STMT_ASSIGNMENT) { //STMT_ASSIGNMENT
        if (exp != NULL) ir = get_Koopa_reg_ir(exp, contex); 
        std::string name = lval->IR();
        if (lval->tag == AST_ARRAY) {
          contex = contex + name;
          name = "@array_element" + std::to_string(KoopaReg_id);
        }
        assert(name[0] == '@'); //必须保证assignment的左边是个变量 
        keyIR = "  store " + ir + ", " + name + "\n";
        return contex + keyIR;
      }
      if (type == STMT_EXECUTE) {
        if (exp != NULL) return exp->IR();
        return "";
      }
      if (type == STMT_IF) {
        if (break_label != "") {
          if (stmt1 != NULL) {
            stmt1->set_break_label(break_label);
            stmt1->set_continue_label(continue_label);
          }
          if (stmt2 != NULL) {
            stmt2->set_break_label(break_label);
            stmt2->set_continue_label(continue_label);
          }
        }
        //STMT_IF
        ++IfStmt_num;
        std::string then_label = "%then_" + std::to_string(IfStmt_num) + "_label";
        std::string else_label = "%else_" + std::to_string(IfStmt_num) + "_label";
        std::string endif_label = "%endif_" + std::to_string(IfStmt_num) + "_label";
        assert(stmt1 != NULL);
        if (stmt2 == NULL) //没有else的时候，条件不满足就结束
          else_label = endif_label; 

        exp->set_exp_type(EXP_CONDITION);
        exp->set_exp_true_label(then_label);
        exp->set_exp_false_label(else_label);
        contex = contex + exp->IR();

        stmt1->set_block_entryname(then_label);
        contex = contex + stmt1->IR();
        contex = contex + "  jump " + endif_label + "\n";
        
        if (stmt2 != NULL) {
          stmt2->set_block_entryname(else_label);
          contex = contex + stmt2->IR();
          contex = contex + "  jump " + endif_label + "\n";
        }
        
        //开启block
        assert(endif_label != "");
        keyIR = endif_label + ":\n";
        return contex + keyIR;
      }
      if (type == STMT_WHILE) {
        ++WhileStmt_num;
        std::string entry_label = "%while_entry" + std::to_string(WhileStmt_num) + "_label";
        std::string body_label = "%while_body" + std::to_string(WhileStmt_num) + "_label";
        std::string end_label = "%endwhile" + std::to_string(WhileStmt_num) + "_label";
        
        contex = contex + "  jump " + entry_label + "\n";
        contex = contex + entry_label + ":\n";
        exp->set_exp_type(EXP_CONDITION);
        exp->set_exp_true_label(body_label);
        exp->set_exp_false_label(end_label);
        contex = contex + exp->IR();

        stmt1->set_block_entryname(body_label);
        stmt1->set_continue_label(entry_label);
        stmt1->set_break_label(end_label);
        contex = contex + stmt1->IR();
        contex = contex + "  jump " + entry_label + "\n";

        //开启block
        assert(end_label != "");
        keyIR = end_label + ":\n";
        return contex + keyIR;
      }
      if (type == STMT_BREAK) {
        contex = contex + "  jump " + break_label + "\n";
        keyIR = "%after_break_block" + std::to_string(++BreakStmt_num) + "_label:\n";
        return contex + keyIR;
      }
      if (type == STMT_CONTINUE) {
        contex = contex + "  jump " + continue_label + "\n";
        keyIR = "%after_continue_block" + std::to_string(++ContinueStmt_num) + "_label:\n";
        return contex + keyIR;
      }

      assert(false);
      return "";
    }
};

class LValAST : public BaseAST {
  public:
    std::string ident;
    exp_type_t type;
    std::string true_label, false_label;

    LValAST() : BaseAST(AST_LVAL) {
      type = EXP_EVALUATION;
      true_label = "", false_label = "";
    }
    void set_exp_type(exp_type_t _type) override {
      type = _type;
    }
    void set_exp_true_label(const std::string& label) override {
      true_label = label;
    }
    void set_exp_false_label(const std::string& label) override {
      false_label = label;
    }
    //需要查表看它是常量还是变量 然后做出反应
    std::string IR() const override { 
      std::string ir = Koopa_Symbol_Table.questIdent(ident);
      assert(ir != ""); //确保这是一个已经声明的变量

      data_t data = Koopa_Symbol_Table.questArray(ident);
      if (data.tag == DATA_ARRAY) { //如果是个数组，这里要的就是数组的指针
        ++KoopaReg_id;
        std::string contex = "  %" + std::to_string(KoopaReg_id) + " = getelemptr " + ir + ", 0\n";
        return contex;
      }
      //如果是数组指针的话，可以load出具体指往下传

      if (type == EXP_EVALUATION) return ir;

      //EXP_CONDITION
      if (ir[0] == '@') { //如果是变量，需要用br
        //先加载
        ++KoopaReg_id;
        std::string contex = "  %" + std::to_string(KoopaReg_id) + " = load " + ir + "\n";
        return contex + "  br %" + std::to_string(KoopaReg_id) + ", " + true_label + ", " + false_label + "\n";
      }
      //如果是常量，用jump
      if (ir != "0") return "  jump " + true_label + "\n";
      return "  jump " + false_label + "\n";
    }
    data_t calculate() const override {
      assert(Koopa_Symbol_Table.questIdent(ident) != ""); //确保这是一个已经声明的变量
      return Koopa_Symbol_Table.questConstant(ident);
    }
};

static int array_lval_num = 0;
class ArrayAST : public BaseAST {
  public:
    std::string ident;
    std::vector<std::unique_ptr<BaseAST>> dim_exps;
    exp_type_t type;
    std::string true_label, false_label;

    ArrayAST() : BaseAST(AST_ARRAY) {
      type = EXP_EVALUATION;
      true_label = "", false_label = "";
    }
    void set_exp_type(exp_type_t _type) override {
      type = _type;
    }
    void set_exp_true_label(const std::string& label) override {
      true_label = label;
    }
    void set_exp_false_label(const std::string& label) override {
      false_label = label;
    }
    //需要查表看它是常量还是变量 然后做出反应
    std::string evaluationIR() const { //返回目标的address
      data_t array_data = Koopa_Symbol_Table.questArray(ident);
      assert(array_data.tag == DATA_ARRAY || array_data.tag == DATA_PTR); //确保这是个已经声明的数组或者数组指针
      //assert(array_data.value.dims->size() == dim_exps.size()); //确保可以定位到一个具体的数组单元
      
      std::string contex = "";
      std::string ir = Koopa_Symbol_Table.questIdent(ident);
      std::string nxtir = "";
      std::string index = "";
      if (array_data.tag == DATA_PTR) {
        contex = contex + "  %" + std::to_string(++KoopaReg_id) + " = load " + ir + "\n";
        ir = "%" + std::to_string(KoopaReg_id); 
      }

      int my_array_lval_num = ++array_lval_num;
      for (int i = 0, sze = dim_exps.size(); i < sze; ++i, ir = nxtir) {
        index = get_Koopa_reg_ir(dim_exps[i], contex);
        if (i == array_data.value.dims->size() - 1) nxtir = "@array_element" + std::to_string(++KoopaReg_id);
        else 
          nxtir = "%array_ptr" + std::to_string(i + 1) + "_" + std::to_string(my_array_lval_num);
        if (i == 0 && array_data.tag == DATA_PTR)
          contex = contex + "  " + nxtir + " = getptr " + ir + ", " + index + "\n";
        else  
          contex = contex + "  " + nxtir + " = getelemptr " + ir + ", " + index + "\n";
      }

      if (dim_exps.size() < array_data.value.dims->size()) {//得到的应该是一个指针而不是一个element
        contex = contex + "  %" + std::to_string(++KoopaReg_id) + " = getelemptr " + ir + ", 0\n";
        contex[0] = 'p';//标注这是一个指针
      }

      return contex;
    }
    std::string IR() const override { 
      std::string contex = evaluationIR();
      if (type == EXP_EVALUATION) return contex;

      std::string ir = "@array_element" + std::to_string(KoopaReg_id);
      //EXP_CONDITION
      //先加载
      ++KoopaReg_id;
      contex = contex + "  %" + std::to_string(KoopaReg_id) + " = load " + ir + "\n";
      return contex + "  br %" + std::to_string(KoopaReg_id) + ", " + true_label + ", " + false_label + "\n";
    }
    data_t calculate() const override { //数组内容不能在编译时计算
      return data_t(DATA_UNKNOWN, 0);
    }
};

static int ConditionExp_num = -1;
class ExpAST : public BaseAST {
  public:
    exp_type_t type;
    std::string true_label, false_label;
    std::unique_ptr<BaseAST> exp1;
    std::string op;
    std::unique_ptr<BaseAST> exp2;

  ExpAST(): BaseAST(AST_EXP) { 
    type = EXP_EVALUATION; 
    true_label = "", false_label = "";
  }
  void set_exp_type(exp_type_t _type) override {
    type = _type;
  }
  void set_exp_true_label(const std::string& label) override {
    true_label = label;
  }
  void set_exp_false_label(const std::string& label) override {
    false_label = label;
  }
  std::string evaluationIR() const { 
    /*EVALUATION expIR是一堆式子，形如
        %1 = -2; 
        %2 = %1 * 2; 
    这样，其中表达式的值放在最后一个临时寄存器里；
    保证最后一个临时寄存器的编号就是最新的reg_id

    注意: evaluationIR也要求短路求值
    */
      
    if (op == "&&" || op == "||") {
      { // $$ 和 ||
        //修改ir1和ir2 从整数变成bool
        std::string contex1 = "", contex2 = "", contex = "";
        std::string ir1 = get_Koopa_reg_ir(exp1, contex1);
        std::string ir2 = get_Koopa_reg_ir(exp2, contex2);
        
        ++KoopaReg_id;
        contex1 = contex1 + "  %" + std::to_string(KoopaReg_id) + " = ne 0, " + ir1 + "\n";
        ir1 = "%" + std::to_string(KoopaReg_id);

        ++ConditionExp_num;
        std::string label_true = "%condition" + std::to_string(ConditionExp_num) + "_true";
        std::string label_false = "%condition" + std::to_string(ConditionExp_num) + "_false";
        std::string label_end = "%condition" + std::to_string(ConditionExp_num) + "_end";
        std::string result_var = "%condition_result" + std::to_string(ConditionExp_num);
        
        contex = contex1;
        contex = contex + "  " + result_var + " = alloc i32\n";
        contex = contex + "  br " + ir1 + ", " + label_true + ", " + label_false + "\n";

        if (op == "||") {
          contex = contex + label_true + ":\n";
          contex = contex + "  store 1, " + result_var + "\n";
          contex = contex + "  jump " + label_end + "\n";
          contex = contex + label_false + ":\n";
          contex = contex + contex2;
          contex = contex + "  %" + std::to_string(++KoopaReg_id) + " = ne 0, " + ir2 + "\n";
          ir2 = "%" + std::to_string(KoopaReg_id);
          contex = contex + "  store " + ir2 + ", " + result_var + "\n";
          contex = contex + "  jump " + label_end + "\n";
        }
        else { // op == "&&"
          ++KoopaReg_id;
          contex = contex + label_true + ":\n";
          contex = contex + contex2;
          contex = contex + "  %" + std::to_string(++KoopaReg_id) + " = ne 0, " + ir2 + "\n";
          ir2 = "%" + std::to_string(KoopaReg_id);
          contex = contex + "  store " + ir2 + ", " + result_var + "\n";
          contex = contex + "  jump " + label_end + "\n";
          contex = contex + label_false + ":\n";
          contex = contex + "  store 0, " + result_var + "\n";
          contex = contex + "  jump " + label_end + "\n";
        }
        contex = contex + label_end + ":\n";
        contex = contex + "  %" + std::to_string(++KoopaReg_id) + " = load " + result_var + "\n";
        return contex;
      }
    }

    std::string contex = "";
    std::string ir1 = "";
    std::string ir2 = "";
    std::string keyIR = "";

    if (exp1 == NULL) { //一元表达式 +(已经处理) - ！
      ir2 = get_Koopa_reg_ir(exp2, contex);

      switch (op[0]) { 
        case '-':  
          keyIR = "sub 0, "+ ir2;
          break;
        case '!':
          keyIR = "eq " + ir2 + ", 0";
          break;
        default: //暂时不会遇到其他内容
          assert(false);
      }
      keyIR = "  %"+std::to_string(++KoopaReg_id) + " = " + keyIR + "\n";
      return contex + keyIR;
    }
    
    ir1 = get_Koopa_reg_ir(exp1, contex);
    ir2 = get_Koopa_reg_ir(exp2, contex);

    if (op.length() == 1) {//二元表达式 * / %  + -  或 < > 
      switch (op[0]) { 
        case '*':  
          keyIR = "mul " + ir1 + ", "+ ir2;
          break;
        case '/':
          keyIR = "div " + ir1 + ", " + ir2;
          break;
        case '%':
          keyIR = "mod " + ir1 + ", " + ir2;
          break;
        case '+':
          keyIR = "add " + ir1 + ", " + ir2;
          break;
        case '-':
          keyIR = "sub " + ir1 + ", " + ir2;
          break;
        case '<':
          keyIR = "lt " + ir1 + ", " + ir2;
          break;
        case '>':
          keyIR = "gt " + ir1 + ", " + ir2;
          break;
        default: //暂时不会遇到其他内容
          assert(false);
      }
      keyIR = "  %"+std::to_string(++KoopaReg_id) + " = " + keyIR + "\n";
      return contex + keyIR;
    }
    {//其他逻辑表达式
      if (op == "<=") keyIR = "le " + ir1 + ", "+ ir2;
      if (op == ">=") keyIR = "ge " + ir1 + ", "+ ir2;
      if (op == "==") keyIR = "eq " + ir1 + ", "+ ir2;
      if (op == "!=") keyIR = "ne " + ir1 + ", "+ ir2;
      // if (op == "&&" || op == "||") {
      //   { // $$ 和 ||
      //     //修改ir1和ir2 从整数变成bool
      //     ++KoopaReg_id;
      //     contex = contex + "  %" + std::to_string(KoopaReg_id) + " = ne 0, " + ir1 + "\n";
      //     ir1 = "%" + std::to_string(KoopaReg_id);
      //     ++KoopaReg_id;
      //     contex = contex + "  %" + std::to_string(KoopaReg_id) + " = ne 0, " + ir2 + "\n";
      //     ir2 = "%" + std::to_string(KoopaReg_id);

      //     if (op == "&&") keyIR = "and " + ir1 + ", "+ ir2;
      //     if (op == "||") keyIR = "or " + ir1 + ", "+ ir2;
      //   }
      // }
      keyIR = "  %"+std::to_string(++KoopaReg_id) + " = " + keyIR + "\n";
      return contex + keyIR;
    }
  }

  std::string IR() const override {
    if (type == EXP_EVALUATION) return evaluationIR();
    /*another case: CONDITION expIR 需要支持短路求值
        不考虑短路求值的情形
          ...
          %n = xxx;  //存表达式的值
          br %n, true_label, false_label
        考虑短路求值的情形（op是&&或者||) 
          ...

      要求一定以jump或br结尾
    */
    std::string contex = "";
    std::string keyIR;
    if (op == "||") {
      assert(exp1 != NULL);
      assert(exp2 != NULL);

      std::string label = "%condition_" + std::to_string(++ConditionExp_num);
      exp1->set_exp_type(EXP_CONDITION);
      exp1->set_exp_true_label(true_label);
      exp1->set_exp_false_label(label);
      contex = contex + exp1->IR();

      assert(label != "");
      contex = contex + label + ":\n";
      exp2->set_exp_type(EXP_CONDITION);
      exp2->set_exp_true_label(true_label);
      exp2->set_exp_false_label(false_label);
      contex = contex + exp2->IR();
    }
    else if (op == "&&") {
      assert(exp1 != NULL);
      assert(exp2 != NULL);

      std::string label = "%condition_" + std::to_string(++ConditionExp_num);
      exp1->set_exp_type(EXP_CONDITION);
      exp1->set_exp_true_label(label);
      exp1->set_exp_false_label(false_label);
      contex = contex + exp1->IR();

      assert(label != "");
      contex = contex + label + ":\n";
      exp2->set_exp_type(EXP_CONDITION);
      exp2->set_exp_true_label(true_label);
      exp2->set_exp_false_label(false_label);
      contex = contex + exp2->IR();
    }
    else { //不需要考虑短路求值的情形
      contex = contex + evaluationIR();
      std::string ir = "%"+ std::to_string(KoopaReg_id);
      keyIR = "  br " + ir + ", " + true_label + ", " + false_label + "\n";
    }
    return contex + keyIR;
  }

  data_t calculate() const override { 
    data_t lhs, rhs, res;
    if (exp1 == NULL) { //一元表达式 +(已经处理) - ！
      rhs = exp2 -> calculate();
      if (rhs.tag == DATA_UNKNOWN) return data_t(DATA_UNKNOWN, 0);
      switch (op[0]) { 
        case '-':  
          res = data_t(DATA_INT, -rhs.value.number);
          break;
        case '!':
          res = data_t(DATA_INT, rhs.value.number == 0);
          break;
        default: //暂时不会遇到其他内容
          assert(false);
      }
      return res;
    }

    lhs = exp1 -> calculate();
    rhs = exp2 -> calculate();
    if (lhs.tag == DATA_UNKNOWN) return data_t(DATA_UNKNOWN, 0);
    if (rhs.tag == DATA_UNKNOWN) return data_t(DATA_UNKNOWN, 0);

    if (op.length() == 1) {//二元表达式 * / %  + -  或 < > 
      switch (op[0]) { 
        case '*':  
          res = data_t(DATA_INT, lhs.value.number * rhs.value.number);
          break;
        case '/':
          res = data_t(DATA_INT, lhs.value.number / rhs.value.number);
          break;
        case '%':
          res = data_t(DATA_INT, lhs.value.number % rhs.value.number);
          break;
        case '+':
          res = data_t(DATA_INT, lhs.value.number + rhs.value.number);
          break;
        case '-':
          res = data_t(DATA_INT, lhs.value.number - rhs.value.number);
          break;
        case '<':
          res = data_t(DATA_INT, lhs.value.number < rhs.value.number);
          break;
        case '>':
          res = data_t(DATA_INT, lhs.value.number > rhs.value.number);
          break;
        default: //暂时不会遇到其他内容
          assert(false);
      }
      return res;
    }
    {//其他逻辑表达式
      if (op == "<=") res = data_t(DATA_INT, lhs.value.number <= rhs.value.number);
      if (op == ">=") res = data_t(DATA_INT, lhs.value.number >= rhs.value.number);
      if (op == "==") res = data_t(DATA_INT, lhs.value.number == rhs.value.number);
      if (op == "!=") res = data_t(DATA_INT, lhs.value.number != rhs.value.number);
      if (op == "&&") res = data_t(DATA_INT, lhs.value.number && rhs.value.number);
      if (op == "||") res = data_t(DATA_INT, lhs.value.number || rhs.value.number);
      return res;
    }
  }
};

class NumberAST : public BaseAST {
  public:
    int int_const;
    exp_type_t type;
    std::string true_label, false_label;

  NumberAST(): BaseAST(AST_NUMBER){
    type = EXP_EVALUATION;
    true_label = "", false_label = "";
  }
  void set_exp_type(exp_type_t _type) override {
    type = _type;
  }
  void set_exp_true_label(const std::string& label) override {
    true_label = label;
  }
  void set_exp_false_label(const std::string& label) override {
    false_label = label;
  }
  std::string IR() const override { 
    if (type == EXP_EVALUATION) return std::to_string(int_const); //evaluation number IR就是数本身
    //EXP_CONDITION
    if (int_const) return "  jump " + true_label + "\n";
    return "  jump " + false_label + "\n";
  }
  data_t calculate() const override{
    return data_t(DATA_INT, int_const);
  }
};

class AggregateAST : public BaseAST {
  public:
    std::vector<std::unique_ptr<BaseAST> > elements;
    dims_t array_value;
    dims_t dims;

    AggregateAST(): BaseAST(AST_AGGREGATE) {
      dims = NULL;
      array_value = NULL;
    }
    ~AggregateAST() {
      if (array_value != NULL) delete array_value;
    }
    void set_array_dims(const dims_t& _dims) override {
      dims = _dims;  
    }
    data_t aggregate_calculate() override { //计算aggregate的值，放到array_value里，向外传指针副本
      int array_len = 1;
      for (auto it = dims->begin(); it != dims->end(); ++it)
        array_len = array_len * (*it);
      array_value = new std::vector<int> ();
      array_value->clear();
      for (int i = 0, sze = elements.size(); i < sze; ++i) { //依次把element放到数组里
        if (elements[i] -> tag == AST_AGGREGATE) {
          //对应一个aggregate 先计算dims信息
          dims_t ele_dims = new std::vector<int> ();
          bool is_aligned = false;
          int len = array_len;
          for (auto it = dims->begin(); it != dims->end(); ++it) {
            if (it != dims->begin() && array_value->size() % len == 0) {//找到一个对齐边界
              is_aligned = true;
              ele_dims->push_back(*it);
            }
            assert(*it != 0); //数组width始终>0
            len = len / (*it);
          }
          assert(is_aligned); //保证对齐，否则语义错误
          elements[i]->set_array_dims(ele_dims);
          data_t ele_value = elements[i] -> aggregate_calculate(); //递归计算
          assert(ele_value.tag == DATA_ARRAY);
          //搬运
          for (auto it = ele_value.value.dims->begin(); it != ele_value.value.dims->end(); ++it)
            array_value->push_back(*it);
        }  
        else { //对应一个int
          data_t ele_value = elements[i] -> calculate();
          if (ele_value.tag == DATA_UNKNOWN) { 
            //一个元素算不出来意味着整个aggregate都UNKNOWN
            return data_t(DATA_UNKNOWN, 0);
          }
          assert(ele_value.tag == DATA_INT);
          array_value->push_back(ele_value.value.number);
        }
      }
      while (array_value->size() < array_len)
        array_value->push_back(0);
      
      assert(array_value->size() == array_len);
      data_t ret;
      ret.tag = DATA_ARRAY;
      ret.value.dims = array_value;
      return ret;
    }

    data_t calculate() const override { 
      data_t ret;
      ret.tag = DATA_ARRAY;
      ret.value.dims = array_value;
      return ret;
    }

    std::string IR() const override { //考虑两种情况
      data_t array_data = calculate();
      assert(array_data.tag == DATA_ARRAY);
      
      std::vector<std::string>* x = new std::vector<std::string> (); ;
      std::vector<std::string>* y = new std::vector<std::string> (); y->clear();
      for (auto it = array_data.value.dims->begin(); it != array_data.value.dims->end(); ++it)
        y->push_back(std::to_string(*it));
      for (auto it = dims->rbegin(); it != dims->rend(); ++it, swap(x, y)) {
        int width = *it, index = 0;
        std::string element_ir = "";
        x->clear();
        for (auto e = y->begin(); e != y->end(); ++e, index = (index + 1) % width) {
          if (index) element_ir = element_ir + ", ";
          element_ir = element_ir + *e;
          if (index == width - 1) {
            x->push_back("{" + element_ir + "}");
            element_ir = "";
          }
        }
      }
      assert(y->size() == 1);
      std::string ir = *(y->begin());
      delete x;
      delete y;
      return ir;
    }
};

class FuncExpAST : public BaseAST {
  public:
    std::string ident;
    std::vector <std::unique_ptr<BaseAST> > params;
    exp_type_t type;
    std::string true_label, false_label;
  
  FuncExpAST(): BaseAST(AST_FUNCEXP){
    type = EXP_EVALUATION;
    true_label = "", false_label = "";
  }
  void set_exp_type(exp_type_t _type) override {
    type = _type;
  }
  void set_exp_true_label(const std::string& label) override {
    true_label = label;
  }
  void set_exp_false_label(const std::string& label) override {
    false_label = label;
  }

  func_info_t get_funcExp_info() const override {
    return Koopa_Symbol_Table.questFunc(ident);
  }
  
  std::string evaluationIR() const {
    //EXP_EVALUATION  如果是有返回值的函数，返回值放在最新的寄存器里；
    std::string contex = "";
    std::string paramsIR = "(";
    std::string ir, keyIR;
    for (int i = 0; i < params.size(); ++i) { 
      ir = get_Koopa_reg_ir(params[i], contex);
      if (i != 0) paramsIR = paramsIR + ", ";
      paramsIR = paramsIR + ir;
    } 
    paramsIR = paramsIR + ")";

    func_info_t func_info = Koopa_Symbol_Table.questFunc(ident);
    assert(func_info.type != FUNCTYPE_INVALID); //要确保这是一个已经定义的函数
    //需要考虑函数类型
    if (func_info.type == FUNCTYPE_INT) {
      ++KoopaReg_id;
      keyIR = "  %" + std::to_string(KoopaReg_id) + " = call @" + ident + paramsIR + "\n";
    }
    else {//FUNCTYPE_VOID
      keyIR = "  call @" + ident + paramsIR + "\n";
    }

    return contex + keyIR;
  }

  std::string IR() const override { //考虑两种情况
    if (type == EXP_EVALUATION) return evaluationIR();
    //EXP_CONDITION

    func_info_t func_info = Koopa_Symbol_Table.questFunc(ident);
    assert(func_info.type == FUNCTYPE_INT); //要确保这是一个有返回值的函数
    
    std::string contex = evaluationIR();
    std::string ir = "%"+ std::to_string(KoopaReg_id);
    std::string keyIR = "  br " + ir + ", " + true_label + ", " + false_label + "\n";
    
    return contex + keyIR;
  }

  data_t calculate() const override{
    return data_t(DATA_UNKNOWN, 0);
  }
};

#endif