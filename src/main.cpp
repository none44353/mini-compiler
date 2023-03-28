#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include "koopa.h"
#include "ast.h"
#include <map>

using namespace std;

// 声明 lexer 的输入, 以及 parser 函数
// 为什么不引用 sysy.tab.hpp 呢? 因为首先里面没有 yyin 的定义
// 其次, 因为这个文件不是我们自己写的, 而是被 Bison 生成出来的
// 你的代码编辑器/IDE 很可能找不到这个文件, 然后会给你报错 (虽然编译不会出错)
// 看起来会很烦人, 于是干脆采用这种看起来 dirty 但实际很有效的手段
extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);

//Visit过程中可能会用到的全局变量
int delta_bp, inst_idx, save_ra_need;

/*Koopa AST中用到的符号表*/
int KoopaReg_id = -1;
int value_i = 0;
Koopa_Symbol_Table_t Koopa_Symbol_Table;
// std::unordered_set <std::string> ident_table; //常量，变量表
// std::unordered_map <std::string, data_t> const_symbol_table; //常数符号表

//Koopa AST中用到的程序
void get_array_init(const dims_t& dims, const dims_t& value, int dim_i, const std::string& ir, std::string& contex) {
  if (dim_i == dims->size()) {
    auto it = value->begin();
    for (int i = 0; i < value_i; ++i) ++it;
    contex = contex + "  store " + std::to_string(*it) + ", " + ir + "\n";
    
    value_i++;
    return;
  }

  auto it = dims->begin();
  for (int i = 0; i < dim_i; ++i) ++it;
  int width = *it;
  for (int i = 0; i < width; ++i) {
    ++KoopaReg_id;
    std::string nir = "%" + std::to_string(KoopaReg_id);
    contex = contex + "  " + nir + " = getelemptr " + ir + ", " + std::to_string(i) + "\n";
    get_array_init(dims, value, dim_i + 1, nir, contex);
  }
}

std::string get_Koopa_reg_ir(const std::unique_ptr<BaseAST>& exp, std::string& contex) { 
  assert(exp != NULL); //要求传入一个非空指针
  std::string ir = "";
  switch (exp -> tag)
  {
    case AST_NUMBER: {
      ir = exp->IR();
      break;
    }
    case AST_EXP: {
      contex = contex + exp->IR();
      ir = "%" + std::to_string(KoopaReg_id);
      break;
    }
    case AST_LVAL: {
      ir = exp -> IR();  //可能是一个常数或者一个形如@x的东西, 【也可能是数组或者数组指针】
      if (ir[0] == ' ') { //这里lval对应的是一个数组name
        contex = contex + ir;
        ir = "%" + std::to_string(KoopaReg_id);
      }
      else if (ir[0] == '@') {
        ++KoopaReg_id;
        contex = contex + "  %" + std::to_string(KoopaReg_id) + " = load " + ir + "\n";
        ir = "%" + std::to_string(KoopaReg_id);
      }
      break;
    }
    case AST_ARRAY: {
      ir = exp -> IR(); //一个指针
      if (ir[0] == 'p') {
        ir[0] = ' ';
        contex = contex + ir;
        ir = "%" + std::to_string(KoopaReg_id);
      }
      else {
        contex = contex + ir;
        ir = "@array_element" + std::to_string(KoopaReg_id);
        ++KoopaReg_id;
        contex = contex + "  %" + std::to_string(KoopaReg_id) + " = load " + ir + "\n";
        ir = "%" + std::to_string(KoopaReg_id);
      }
      break;
    }
    case AST_FUNCEXP: {
      func_info_t func_info = exp->get_funcExp_info();
      assert(func_info.type == FUNCTYPE_INT);  //需要保证是一个返回值不是void的函数
      contex = contex + exp -> IR();
      ir = "%" + std::to_string(KoopaReg_id);
      break;
    }
    default: //理论上不会有其他情况
      assert(false);
  }
  return ir;
}

/*RISCV转译中用到的符号表*/
typedef enum {
  SYMBOL_VALUE, SYMBOL_SHIFT,
}symbol_table_data_tag_t;

struct symbol_table_data_t {
  symbol_table_data_tag_t tag;
  union {
    int value;
    int shift;
  }data;
  symbol_table_data_t() {}
  symbol_table_data_t(symbol_table_data_tag_t _tag, int _value) {
    tag = _tag; data.value = _value;
  }
};
std::map <koopa_raw_value_t, symbol_table_data_t> symbol_table;

size_t calculate_size(const koopa_raw_type_t& type) {
  if (type->tag == KOOPA_RTT_INT32) return 4;
  if (type->tag == KOOPA_RTT_POINTER) return 4;
  if (type->tag == KOOPA_RTT_ARRAY) 
    return type->data.array.len * calculate_size(type->data.array.base);
  
  assert(false); //理论上不会有其他情况
}

//定义访问内存Koopa的一套函数, 在访问过程中输出
void Visit(const koopa_raw_program_t &program, std::string& code);
void Visit(const koopa_raw_slice_t &slice, std::string& code);
void Visit(const koopa_raw_function_t &func, std::string& code);
void Visit(const koopa_raw_basic_block_t &bb, std::string& code);
void Visit(const koopa_raw_value_t &value, std::string& code);
std::string shift_from_sp(const int& shift, std::string& contex);

// 访问 raw program
void Visit(const koopa_raw_program_t &program, std::string& code) {
  // 执行一些其他的必要操作
  symbol_table.clear();
  // 访问所有全局变量
  code = code + "  .data\n";
  
  // for (size_t j = 0; j < program.values.len; ++j) {
  //   koopa_raw_value_t value = (koopa_raw_value_t) program.values.buffer[j];
  //   koopa_raw_type_t type = value->ty;
  //   std::cerr << "type tag(" << type->tag << ") " << calculate_size(type->data.pointer.base) << std::endl;
  //   while (type ->tag != KOOPA_RTT_INT32) {
  //     std::cerr << "type tag=" << type->tag << ", ";;
  //     if (type->tag==KOOPA_RTT_ARRAY) {
  //       std::cerr << type->data.array.len;
  //       type = type->data.array.base;
  //     }
  //     else type = type->data.pointer.base;
  //     std::cerr << endl;
  //   }
  //   std::cerr << "global inst(" << value -> kind.tag << ")" <<std::endl;
  // }
  Visit(program.values, code);
  // 访问所有函数
  code = code + "\n  .text\n";
  Visit(program.funcs, code);
}

// 访问 raw slice
void Visit(const koopa_raw_slice_t &slice, std::string& code) {
  for (size_t i = 0; i < slice.len; ++i) {
    auto ptr = slice.buffer[i];
    switch (slice.kind) { // 根据 slice 的 kind 决定将 ptr 视作何种元素
      case KOOPA_RSIK_FUNCTION:  // 访问函数
        Visit(reinterpret_cast<koopa_raw_function_t>(ptr), code);
        break;
      case KOOPA_RSIK_BASIC_BLOCK: // 访问基本块
        Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr), code);
        break;
      case KOOPA_RSIK_VALUE: // 访问指令
        Visit(reinterpret_cast<koopa_raw_value_t>(ptr), code);
        break;
      default: // 我们暂时不会遇到其他内容, 于是不对其做任何处理
        assert(false);
    }
  }
}

// 访问函数
void Visit(const koopa_raw_function_t &func, std::string& code) {
  if (func->bbs.len == 0) { //库函数
    return;
  }

  // 输出 func->name形如@main
  code = code + "  .globl " + std::string(func->name + 1) + "\n"; //先默认所有函数都是globl的
  code = code + std::string(func->name + 1) + ":\n"; //输出函数入口标记
  //遍历函数内指令，求需要的栈空间大小
  int inst_num = 0; //有留栈空间的指令
  inst_num = inst_num + func->params.len; //给函数自身参数读取分配的栈空间
  int callee_arg_num = 0;
  save_ra_need = false;
  for (size_t j = 0; j < func->bbs.len; ++j) {
    assert(func->bbs.kind == KOOPA_RSIK_BASIC_BLOCK);
    koopa_raw_basic_block_t bb = (koopa_raw_basic_block_t) func->bbs.buffer[j];
    std::cerr << "--block(" << bb->name << ")--\n";
    for (size_t k = 0; k < bb -> insts.len; ++k) {
      assert(bb->insts.kind == KOOPA_RSIK_VALUE);
      koopa_raw_value_t value = (koopa_raw_value_t) bb -> insts.buffer[k];
      std::cerr << "inst(" << value -> kind.tag << ")" <<std::endl;
      if (value -> kind.tag == KOOPA_RVT_ALLOC) {
        koopa_raw_type_t type = value->ty;
        inst_num += calculate_size(type->data.pointer.base) / 4;
      }
      if (value -> kind.tag == KOOPA_RVT_LOAD) ++inst_num;
      if (value -> kind.tag == KOOPA_RVT_GET_PTR) ++inst_num;
      if (value -> kind.tag == KOOPA_RVT_GET_ELEM_PTR) ++inst_num;
      if (value -> kind.tag == KOOPA_RVT_BINARY) ++inst_num;

      //如果有函数调用，计算放callee参数需要的栈空间
      if (value -> kind.tag == KOOPA_RVT_CALL) { 
        save_ra_need = true; //需要保存ra
        koopa_raw_function_t callee = value->kind.data.call.callee;
        int arg_len = callee->params.len;
        callee_arg_num = max(callee_arg_num, max(arg_len - 8, 0));

        koopa_raw_type_t callee_type = value->kind.data.call.callee->ty;
        if (callee_type->tag != KOOPA_RTT_UNIT) { //如果不是void, 将会把返回值存在栈中
          ++inst_num;
        }
      }
              /* value是value_data的指针
              struct koopa_raw_value_data {
                koopa_raw_type_t ty;   /// Type of value.
                const char *name;   /// Name of value, null if no name.
                koopa_raw_slice_t used_by;  /// Values that this value is used by.
                koopa_raw_value_kind_t kind;  /// Kind of value.
              };
                kind_t包括tag和一个union
                我们的目标是数有多少个需要占用内存的指令
              */
    }
  }
  inst_num = inst_num + callee_arg_num + (save_ra_need ? 1 : 0);
  int32_t stack_space = inst_num * 4;
  stack_space = ((stack_space - 1) / 16 + 1) * 16; //16对齐 (因为取整是向零取整，在scapce=0的时候会变成16)
  delta_bp = stack_space;
//  code = code + "  addi sp, sp, " + std::to_string(-delta_bp) + "\n";
  code = code + "  li t4, " + std::to_string(-delta_bp) + "\n";
  code = code + "  add sp, sp, t4\n"; 
  if (save_ra_need) {
    //code = code + "  sw ra, " + std::to_string(delta_bp - 4) + "(sp)\n";
    std::string sp_str = shift_from_sp(delta_bp - 4, code);
    code = code + "  sw ra, " + sp_str + "\n";
  }

  inst_idx = callee_arg_num;  //把栈底为callee留出来
  // TBD 符号表初始化 symbol_table.clear();
  std::string args_code = "";
  Visit(func->params, args_code); //访问参数
  std::string func_code = "";
  Visit(func->bbs, func_code); // 访问所有基本块
  code = code + args_code + func_code;
}

inline std::string get_block_name(const koopa_raw_basic_block_t &bb) {
  return std::string(bb->name + 1);
}
// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb, std::string& code) {
  // 执行一些其他的必要操作
  std::string block_name = get_block_name(bb);
  if (block_name != "entry")
    code = code + block_name + ":\n";
  // 访问所有指令
  Visit(bb->insts, code);
}

std::string shift_from_sp(const int& shift, std::string& contex) {
  if (-2048 <= shift && shift < 2048) {
    return std::to_string(shift) + "(sp)";
  }
  contex = contex + "  li t4, " + std::to_string(shift) + "\n";
  contex = contex + "  add t4, t4, sp\n";
  return "0(t4)";
}

std::string get_addr_riscv(const std::string& reg, const koopa_raw_value_t&value) {
  //把address加载到寄存器reg里
  const auto &kind = value->kind; //判断是常数还是一个变量
  if (kind.tag == KOOPA_RVT_GLOBAL_ALLOC) { //如果是全局变量
    std::string code = "  la " + reg + ", " + string(value->name + 1) + "\n"; 
    return code; 
  }
  //否则可以在符号表里查到
  assert(symbol_table.find(value) != symbol_table.end());
  auto symbol = symbol_table[value];

  //SYMBOL_SHIFT
  std::string code = "  li t4, " + std::to_string(symbol.data.shift) + "\n";
  //std::string data_str = std::to_string(symbol.data.shift);
  return code + "  add " + reg + ", sp, t4\n";
}

std::string load_riscv(const std::string& reg, const koopa_raw_value_t& value) {
  //注意这个value是一个address, 目标是从address里getvalue
  const auto &kind = value->kind; //判断是常数还是一个变量
  std::string data_str = "";
  if (kind.tag == KOOPA_RVT_GLOBAL_ALLOC) { //如果是全局变量
    std::string code = "  la " + reg + ", " + string(value->name + 1) + "\n"; 
    code = code + "  lw " + reg + ", " + "0(" + reg + ")" + "\n";
    return code; 
  }
  //否则可以在符号表里查到
  assert(symbol_table.find(value) != symbol_table.end());
  auto symbol = symbol_table[value];
  //SYMBOL_SHIFT
  if (kind.tag == KOOPA_RVT_ALLOC) { //如果是局部变量
    std::string ret = "";
    std::string sp_str = shift_from_sp(symbol.data.shift, ret);
    ret = ret + "  lw " + reg + ", " +sp_str + "\n";
    return ret;
    //data_str = std::to_string(symbol.data.shift);
    //return "  lw " + reg + ", " + data_str + "(sp)\n";
  }
  //如果value是算出来的指针
  //data_str = std::to_string(symbol.data.shift);
  //std::string code = "  lw " + reg + ", " + data_str + "(sp)\n";
  std::string code = "";
  std::string sp_str = shift_from_sp(symbol.data.shift, code);
  code = code + "  lw " + reg + ", " + sp_str + "\n";
  code = code + "  lw " + reg + ", " + "0(" + reg + ")" + "\n";
  return code;
}

std::string load_from_stack_riscv(const std::string& reg, const koopa_raw_value_t& value) {
  const auto &kind = value->kind; //判断是常数还是一个变量
  std::string data_str = "";
  if (kind.tag == KOOPA_RVT_INTEGER) {
    Visit(value, data_str);
    return "  li " + reg + ", " + data_str + "\n";
  }
  //否则保证可以在符号表里查到
  assert(symbol_table.find(value) != symbol_table.end());
  auto symbol = symbol_table[value];
  
  if (symbol.tag == SYMBOL_VALUE) { //如果是常数
    data_str = std::to_string(symbol.data.value);
    return  "  li " + reg + ", " + data_str + "\n";
  }
  //SYMBOL_SHIFT
  std::string ret = "";
  std::string sp_str = shift_from_sp(symbol.data.shift, ret);
  ret = ret +  "  lw " + reg + ", " + sp_str + "\n";
  return ret;
  //data_str = std::to_string(symbol.data.shift);
  //return "  lw " + reg + ", " + data_str + "(sp)\n";
}

std::string store_into_stack_riscv(const std::string& reg, const koopa_raw_value_t& value) {
  assert(symbol_table.find(value) != symbol_table.end());  //要确保可以在符号表里查到value
  auto symbol = symbol_table[value];
  assert(symbol.tag == SYMBOL_SHIFT);  //并且不是常数
  //SYMBOL_SHIFT
  std::string ret = "";
  std::string sp_str = shift_from_sp(symbol.data.shift, ret);
  ret = ret + "  sw " + reg + ", " + sp_str + "\n";
  return ret;
  //return "  sw " + reg + ", " + std::to_string(symbol.data.shift) + "(sp)\n";
}

std::string store_riscv(const std::string& reg, const koopa_raw_value_t& value) {
  const auto &kind = value->kind; //判断是常数还是一个变量
  if (kind.tag == KOOPA_RVT_GLOBAL_ALLOC) { //如果是全局变量
    std::string code = "  la t3, " + string(value->name + 1) + "\n"; 
    code = code + "  sw " + reg + ", " + "0(t3)" + "\n";
    return code;
  }
  assert(symbol_table.find(value) != symbol_table.end());  //要确保可以在符号表里查到value
  auto symbol = symbol_table[value];
  assert(symbol.tag == SYMBOL_SHIFT);  //并且不是常数
  //SYMBOL_SHIFT
  if (kind.tag == KOOPA_RVT_ALLOC) { //如果是局部变量
    std::string ret = "";
    std::string sp_str = shift_from_sp(symbol.data.shift, ret);
    ret = ret + "  sw " + reg + ", " + sp_str + "\n";
    return ret;
    //return "  sw " + reg + ", " + std::to_string(symbol.data.shift) + "(sp)\n";
  }
  //KOOPA_RVT_GET_ELEM_PTR  
  //如果value是个算出来的指针, 那shift是指针存在栈上的为止
  //std::string code = "  lw t3, " + std::to_string(symbol.data.shift) + "(sp)\n";
  std::string code ="";
  std::string sp_str = shift_from_sp(symbol.data.shift, code);
  code = code + "  lw t3, " + sp_str + "\n";
  code = code + "  sw " + reg + ", 0(t3)\n";
  return code;
}

std::string get_callee_arg_reg(const koopa_raw_func_arg_ref_t &value, std::string& code) {
  std::string ir;
  if (value.index < 8) ir = "a" + std::to_string(value.index);
  else {
    int shift = delta_bp + (value.index - 8) * 4;
    code = code + "  lw t0, " + std::to_string(shift) + "(sp)\n";
    ir = "t0";
  }
  return ir;
}

void Print(const koopa_raw_aggregate_t &value, std::string& code) {
  for (int i = 0; i < value.elems.len; ++i) {
    assert(value.elems.kind == KOOPA_RSIK_VALUE);
    koopa_raw_value_t ele = (koopa_raw_value_t)value.elems.buffer[i];
    if (ele->kind.tag == KOOPA_RVT_INTEGER)
      code = code + "  .word " + std::to_string(ele->kind.data.integer.value) + "\n";
    else { //KOOPA_RVT_AGGREGATE
      Print(ele->kind.data.aggregate, code);
    }
  }
}

void Print(const koopa_raw_global_alloc_t &value, const size_t sze, std::string& code) { 
  koopa_raw_value_t init = value.init;
  if (init->kind.tag == KOOPA_RVT_ZERO_INIT) {
    code = code + "  .zero " + std::to_string(sze) + "\n";
  }
  else if (init->kind.tag == KOOPA_RVT_INTEGER) {
    code = code + "  .word " + std::to_string(init->kind.data.integer.value) + "\n";
  }
  else Visit(init, code);

  return;
}

void Print(const koopa_raw_binary_t &value, std::string& code) { //指令返回值放在t2里
  //value.lhs value.op value.rhs  
  //令t0 = lhs, t1 = rhs
  //t2 = op t0, t1
  code = code + load_from_stack_riscv("t0", value.lhs);
  code = code + load_from_stack_riscv("t1", value.rhs);

  switch (value.op)
  {
    case KOOPA_RBO_NOT_EQ:
      code = code + "  xor t2, t0, t1\n" + "  snez t2, t2\n";
      break;
    case KOOPA_RBO_EQ:
      code = code + "  xor t2, t0, t1\n" + "  seqz t2, t2\n";
      break;
    case KOOPA_RBO_GT:
      code = code + "  sgt t2, t0, t1\n";
      break;
    case KOOPA_RBO_LT:
      code = code + "  slt t2, t0, t1\n";
      break;
    case KOOPA_RBO_GE:
      code = code +  "  slt t2, t0, t1\n" + "  seqz t2, t2\n";
      break;
    case KOOPA_RBO_LE:
      code = code +  "  sgt t2, t0, t1\n" + "  seqz t2, t2\n";
      break;
    case KOOPA_RBO_ADD:
      code = code +  "  add t2, t0, t1\n";
      break;
    case KOOPA_RBO_SUB:
      code = code +  "  sub t2, t0, t1\n";
      break;
    case KOOPA_RBO_MUL:
      code = code + "  mul t2, t0, t1\n";
      break;
    case KOOPA_RBO_DIV:
      code = code + "  div t2, t0, t1\n";
      break;
    case KOOPA_RBO_MOD:
      code = code + "  rem t2, t0, t1\n";
      break;
    case KOOPA_RBO_AND:
      code = code + "  and t2, t0, t1\n";
      break;
    case KOOPA_RBO_OR:
      code = code + "  or t2, t0, t1\n";
      break;
    case KOOPA_RBO_XOR:
      code = code + "  xor t2, t0, t1\n";
      break;
    case KOOPA_RBO_SHL:
      code = code + "  sll t2, t0, t1\n";
      break;
    case KOOPA_RBO_SHR:
      code = code + "  srl t2, t0, t1\n";
      break;
    case KOOPA_RBO_SAR:
      code = code + "  sra t2, t0, t1\n";
      break;
    default:
      assert(false);
      break;
  }
  return;
}

void Print(const koopa_raw_get_ptr_t &value, std::string& code) {
  code = code + load_from_stack_riscv("t0", value.src);
  code = code + load_from_stack_riscv("t1", value.index);
  koopa_raw_type_t ptr_type = value.src->ty->data.pointer.base;
  code = code + "  li t2, " + std::to_string(calculate_size(ptr_type)) + "\n";
  code = code + "  mul t1, t1, t2\n";
  code = code + "  add t0, t0, t1\n";
}

void Print(const koopa_raw_get_elem_ptr_t &value, std::string& code) {
  if (value.src->name != NULL && value.src -> name[0] == '@') //如果src是一个全局/局部数组
    code = code + get_addr_riscv("t0", value.src);
  else 
    code = code + load_from_stack_riscv("t0", value.src);

  code = code + load_from_stack_riscv("t1", value.index);
  koopa_raw_type_t ptr_type = value.src->ty->data.pointer.base;
  assert(ptr_type->tag == KOOPA_RTT_ARRAY);
  code = code + "  li t2, " + std::to_string(calculate_size(ptr_type->data.array.base)) + "\n";
  code = code + "  mul t1, t1, t2\n";
  code = code + "  add t0, t0, t1\n";
}

void Print(const koopa_raw_branch_t &value, std::string& code) {
    code = code + load_from_stack_riscv("t0", value.cond);
    code = code + "  bnez t0, " + get_block_name(value.true_bb) + "\n";
    code = code + "  j " + get_block_name(value.false_bb) + "\n";
    return;
}

void Print(const koopa_raw_jump_t &value, std::string& code) {
    code = code + "  j " + get_block_name(value.target) + "\n";
    return;
}

std::string get_caller_arg_reg(size_t index) {
  std::string ir = "t0";
  if (index < 8) ir = "a" + std::to_string(index);
  return ir;
}

void Print(const koopa_raw_call_t &value, std::string& code) {
    //参数准备
    for (int i = 0; i < value.args.len; ++i) {
      assert(value.args.kind == KOOPA_RSIK_VALUE);
      koopa_raw_value_t arg = (koopa_raw_value_t)value.args.buffer[i];
      code = code + load_from_stack_riscv(get_caller_arg_reg(i), arg);
      if (i >= 8) { //已经load参数到t0里了，现在要从t0转移到栈上
        int shift = (i - 8) * 4;
        code = code + "  sw t0, " + std::to_string(shift) + "(sp)\n";
      }
    }
    std::string callee_name = std::string(value.callee->name + 1);
    code = code + "  call " + callee_name + "\n";
    return;
}

void Print(const koopa_raw_return_t &value, std::string& code) {
    if (value.value != NULL) { //如果有返回值
      code = code + load_from_stack_riscv("a0", value.value);
    }
    if (save_ra_need) {
      //code = code + "  lw ra, " + std::to_string(delta_bp - 4) + "(sp)\n";
      std::string sp_str = shift_from_sp(delta_bp - 4, code);
      code = code + "  lw ra, " + sp_str + "\n";
    }
    code = code + "  li t4, " + std::to_string(delta_bp) + "\n";
    code = code + "  add sp, sp, t4\n";
    code = code + "  ret\n";
    return;
}

// 访问指令
void Visit(const koopa_raw_value_t &value, std::string& code) {
  const auto &kind = value->kind; // 根据指令类型判断后续需要如何访问
  std::cerr << "visiting inst " << kind.tag << std::endl;
  switch (kind.tag) {
    case KOOPA_RVT_INTEGER: {// 访问 integer 指令
      //Visit(kind.data.integer);
      code = code + std::to_string(kind.data.integer.value);
      break;
    }

    case KOOPA_RVT_AGGREGATE: {
      Print(kind.data.aggregate, code);
      break;
    }

    case KOOPA_RVT_FUNC_ARG_REF: {//处理函数参数
      //为参数创建符号表
      symbol_table[value] = symbol_table_data_t(SYMBOL_SHIFT, inst_idx * 4);
      ++inst_idx;
      //把参数值放到栈里
      code = code + store_into_stack_riscv(get_callee_arg_reg(kind.data.func_arg_ref, code), value);
      break;
    }

    case KOOPA_RVT_ALLOC: {
      koopa_raw_type_t type = value->ty;
      int delta_idx = calculate_size(type->data.pointer.base) / 4; 
      //为alloc指令的返回值创建符号表项
      symbol_table[value] = symbol_table_data_t(SYMBOL_SHIFT, inst_idx * 4);
      inst_idx += delta_idx;
      break;
    }

    case KOOPA_RVT_GLOBAL_ALLOC: {
      std::string name = std::string(value->name + 1);
      code = code + "  .globl " + name + "\n";
      code = code + name + ":\n";
      Print(kind.data.global_alloc, calculate_size(value->ty->data.pointer.base), code);
      break;
    }

    case KOOPA_RVT_LOAD: {
      code = code + load_riscv("t0", kind.data.load.src);
      //为load指令的返回值创建符号表项
      symbol_table[value] = symbol_table_data_t(SYMBOL_SHIFT, inst_idx * 4);
      ++inst_idx;
      //把返回值放到栈里
      code = code + store_into_stack_riscv("t0", value);
      break;
    }
    
    case KOOPA_RVT_STORE: {
      //std::cerr <<"visit store_instruct " ;
      //std::cerr << std::string(kind.data.store.dest->name) << " " ;
      //std::cerr << kind.data.store.dest->kind.tag << " ";
      //std::cerr << kind.data.store.dest->ty->tag << std::endl;
      //std::fflush(stderr);
      code = code + load_from_stack_riscv("t0", kind.data.store.value);
      code = code + store_riscv("t0", kind.data.store.dest);
      break;
    }

    case KOOPA_RVT_GET_PTR: {
      Print(kind.data.get_ptr, code);
      //为get_elem_ptr指令的返回值创建符号表项
      symbol_table[value] = symbol_table_data_t(SYMBOL_SHIFT, inst_idx * 4);
      ++inst_idx;
      //把返回值放到栈里
      code = code + store_into_stack_riscv("t0", value);
      break;
    }

    case KOOPA_RVT_GET_ELEM_PTR: {
       Print(kind.data.get_elem_ptr, code);
       //为get_elem_ptr指令的返回值创建符号表项
       symbol_table[value] = symbol_table_data_t(SYMBOL_SHIFT, inst_idx * 4);
       ++inst_idx;
       //把返回值放到栈里
       code = code + store_into_stack_riscv("t0", value);
      break;
    }

    case KOOPA_RVT_BINARY: { //访问二元运算指令
      Print(kind.data.binary, code);
      //为binary指令的返回值创建符号表项
      symbol_table[value] = symbol_table_data_t(SYMBOL_SHIFT, inst_idx * 4);
      ++inst_idx;
      //把返回值放到栈里
      code = code + store_into_stack_riscv("t2", value);
      break;
    }
    
    case KOOPA_RVT_BRANCH: {
      Print(kind.data.branch, code);
      break;
    }

    case KOOPA_RVT_JUMP: {
      Print(kind.data.jump, code);
      break;
    }

    case KOOPA_RVT_CALL: {
      Print(kind.data.call, code);
      func_info_t info = Koopa_Symbol_Table.questFunc(std::string(kind.data.call.callee->name + 1));
      assert(info.type != FUNCTYPE_INVALID);
      if (info.type == FUNCTYPE_INT) { //如果返回值是int
        //为函数调用返回值创建符号表项
        symbol_table[value] = symbol_table_data_t(SYMBOL_SHIFT, inst_idx * 4);
        ++inst_idx;
        //把返回值放到栈里
        code = code + store_into_stack_riscv("a0", value);
      }
      break;
    }

    case KOOPA_RVT_RETURN: { // 访问 return 指令
      //此时kind.data包含一个koopa_raw_return_t类型的ret
      Print(kind.data.ret, code); //递归访问输出这个值
      break;
    }

    default: // 其他类型暂时遇不到
      assert(false);
  }
}

std::string parsingIR(const char* str) {//把文本形式的IR转换成内存形式并生成RISC-V
  // 解析字符串 str, 得到 Koopa IR 程序
  koopa_program_t program;
  koopa_error_code_t ret = koopa_parse_from_string(str, &program);
  assert(ret == KOOPA_EC_SUCCESS);  // 确保解析时没有出错
  // 创建一个 raw program builder, 用来构建 raw program
  koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
  // 将 Koopa IR 程序转换为 raw program
  koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
  // 释放 Koopa IR 程序占用的内存
  koopa_delete_program(program);

  std::string riscv = "";
  // 处理 raw program ---- 访问它并输出对应的RISC-V指令
  Visit(raw, riscv);

  // 处理完成, 释放 raw program builder 占用的内存
  // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
  // 所以不要在 raw program 处理完毕之前释放 builder
  koopa_delete_raw_program_builder(builder);
  return riscv;
}

int main(int argc, const char *argv[]) {
  // 解析命令行参数. 测试脚本/评测平台要求你的编译器能接收如下参数:
  // compiler 模式 输入文件 -o 输出文件
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  // 打开输入文件, 并且指定 lexer 在解析的时候读取这个文件
  yyin = fopen(input, "r");
  assert(yyin);

  // parse input file
  unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  assert(!ret);
  //std::cerr << "parsing successfully!\n";
  
  std::string ir = ast->IR();
  std::cerr << ir << std::endl;
  if (string(mode) == "-koopa") {
    freopen(output, "w", stdout);
    std::cout << ir << std::endl;
    fclose(stdout);
    return 0;
  }
  
  if (string(mode) == "-riscv" || string(mode) == "-perf") {
    std::string riscv = parsingIR(ir.c_str());
    std::cerr << std::endl << "--riscv--\n" << riscv << "\n--------\n";
    freopen(output, "w", stdout);
    std::cout << riscv;
    fclose(stdout);
  }
    return 0;
}
