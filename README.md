# Ying's mini-compiler

致看到这个课程项目的后来人：很高兴在这里遇见您！如果您是正在完成课程作业的同学，请您在借鉴我的编译器实现以前，先尝试自己的方案。如果您在经过独立思考后仍遇到困难，欢迎借鉴我的思路。本项目是一个很好的了解编译器实践机会，它帮助我们从实践角度深入了解编译原理。请遵守学术规范和课程规定，**切勿代码抄袭。**

## 一、编译器概述

**1、基本功能**

​	我的编译器实现了在线文档中``Lv1``-``Lv9``的所有功能，它能够将所有本地样例正确地从SysY语言编译到Koopa IR，再从Koopa IR生成到RISC-V。它在在线测试平台上可以通过绝大多数测试样例，在SysY$\to$Koopa IR部分得分99.55，在SysY$\to$RISC-V汇编部分得分99.09。

**2、主要特点**

- 两步走的编译器
  - 我的编译器按照在线文档的主要思路设计，分成两个部分：
    - 第一部分：将SysY编译到Koopa IR
      - 这一部分包括词法分析生成token流、语法分析构造抽象语法树、遍历抽象语法树生成文本形式的Koopa IR三个子部分。
    - 第二部分：将Koopa IR生成到RISC-V汇编
      - 这一部分包括生成内存形式的Koopa IR(由koopa.h库函数实现)和遍历内存形式Koopa IR生成RISC-V文件两个子部分。
- 面向对象的数据管理
  - 我的代码将语法树节点、作用域和符号表这些功能模块抽象、封装为类，让代码结构简洁明了。

- 类型检查
  - 在生成Koopa IR文本时检查常量可以在编译期被求值，应用``assert()``语句排查非法结构。
- 简单但还算完整
  - 我没有聚焦编译器优化部分，只完成了编译器的基本功能。除去部分corner case，这个编译器还算完整，这也是报告标题“麻雀虽小五脏俱全”的来源。

## 二、编译器设计

**1、主要模块组成**

- 用Flex来描述词法分析器，规定token的形式和种类，见``sysy.l``。
- 用Bison来描述语法分析器，规定产生式和产生式对应的用于生成语法树的动作规则，见``sysy.y``。
- 语法分析树以及文本Koopa IR生成，见``ast.h``。
  - 定义各种AST类以及用于生成对应的Koopa IR文本的类函数``std::string IR() const = 0 {...}``，见``ast.h``。
  - 定义生成Koopa IR时用到的符号表和作用域链表结构，见``ast.h``的``Koopa_Symbol_Table_t``类。
- Koopa IR形式转换以及RISC-V汇编生成，见``main.cpp``中的``parsingIR(const char* str)``。
  - 利用库函数将文本形式的Koopa IR转换为内存形式，调用遍历函数生成RISC-V汇编。见``ast.h``中的``std::string parsingIR(const char* str) ``。
  - 遍历内存形式Koopa IR，并将各类具体指令转换成RISC-V汇编。见``main.cpp``中的``void Visit(...)``重载函数以及``void Print(···)``重载函数。
- 以上四个部分在``main.cpp``的主函数``main()``中被关联起来

**2、主要数据结构**

- 各种AST类

  ``ast.h``中定义了不同语法分析树节点的类型（如派生类``CompUnitAST``，``FuncDefAST``，``BlockAST``等），它们都建立在节点类型基类``BaseAST``的基础上。

  ```c++
  class BaseAST {... // 所有 AST 的基类
  };
  
  class CompUnitAST : public BaseAST {...};
  class FuncFParamAST : public BaseAST {...};
  class FuncDefAST : public BaseAST {...};
  class FuncTypeAST : public BaseAST {...};
  class BlockAST : public BaseAST {...};
  class DeclAST : public BaseAST {...};
  class BTypeAST : public BaseAST {...};
  class ArrayDefAST : public BaseAST {...};
  class DefAST : public BaseAST {...};
  class StmtAST : public BaseAST {...};
  class LValAST : public BaseAST {...};
  class ArrayAST : public BaseAST {...};
  class ExpAST : public BaseAST {...};
  class NumberAST : public BaseAST {...};
  class AggregateAST : public BaseAST {...};
  class FuncExpAST : public BaseAST {...};
  ```

  - 每个节点都有一个enum（``AST_tag_t``）``tag``来提示它是哪一派生类，也都会有一个``IR()``类函数来生成该节点的语法分析树子树对应的文本Koopa IR。

    ```c++
    class BaseAST {
     public:
      AST_tag_t tag;
      virtual std::string IR() const = 0;
      ...
     };
    ```

    - 每个派生类AST就是抽象语法树上的一种节点类型，对应一个EBNF的非终结符（但不是每个非终结符都有对应的AST类）。类里包含语法树上子节点的智能指针，以及生成文本形式Koopa IR所要用到的其他信息和辅助函数。

      以DeclAST为例，它对应以下语法

      ```bash
      Decl          ::= ConstDecl | VarDecl;
      ConstDecl     ::= "const" BType ConstDef {"," ConstDef} ";";
      VarDecl       ::= BType VarDef {"," VarDef} ";";
      ```

      它包含：

      - 信息``type``说明这个节点对应的声明对象是[局部常量/局部变量/全局常量/全局变量]。
      - 类型指针``btype``，具体定义的指针``def``。

      该节点有供其父节点调用的辅助函数``set_decl_global()``，这是因为一个声明是全局的还是局部的要根据它的上下文确定。

      ```c++
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
      ```

- Koopa符号表结构``Koopa_Symbol_Table_t``

  - 该结构包含一个作用域链表和全局函数表。

  - 每个作用域内又维护了作用域内部的符号表、常量表和数组表。其中常量表存储常量的具体数值；数组表存储数组的维数、每一维的大小等信息。

  ```c++
  class Koopa_Symbol_Table_t {
   public:
    int scope_num;
    class scope_symbol_table_t { //为了维护符号表，建立链状作用域符号表类
      public:
        int index;
        std::unordered_set <std::string>* ident_table;//常量，变量符号表考虑常量数组和变量数组
        std::unordered_map <std::string, data_t>* const_symbol_table; //常数符号表
        std::unordered_map <std::string, data_t>* array_symbol_table; //数组符号表
        scope_symbol_table_t* father_scope;//指向链表的前驱元素
        ...
    };
    std::unordered_map <std::string, func_info_t>* func_table; //函数表，只要全局一个
    scope_symbol_table_t* current_scope;
    ...
  };
  ```

- RISC-V符号表``std::map <koopa_raw_value_t, symbol_table_data_t> symbol_table;``

  - 我的编译器没有寄存器优化，所有的运算结果都会被放到栈中。RISC-V符号表把一个Koopa IR指令映射到一个栈偏移量上，告诉编译器指令结果应该到栈上的哪个位置找。

**3、主要算法设计考虑**

- 为了尽可能少地重构代码，我的语法分析树结构尽可能和文档给出的EBNF（扩展巴科斯范式）一致。
- 为了提高代码的可扩展性，我采用面向对象的编程方法。
- 由于写代码过程中缺乏对后面关卡的理解和整体分析，我不可避免地遇到需要重构代码的情况，我采用尽可能规避大改动的原则。如，
  - 在``Lv9``之前我都没有考虑指针问题，为方便处理，我在``main.cpp``中定义了``std::string load_riscv(const std::string& reg, const koopa_raw_value_t& value)``函数用于生成将某个符号的值加载到``reg``寄存器中的代码，利用``value->kind``区分不同符号。
  - 在考虑指针以后，``load_riscv``就有了二义：
    - 如果``value``对应的是一个指针，我可能需要指针的值，也可能需要该指针指向地址单元上的内容。
    - 为了区分这两种情况，我把``load_riscv``分成两个函数：``load_riscv``和``load_from_stack_riscv``（``load_riscv``相比于``load_from_stack_riscv``多出一个寻址步骤），并把所有对``load_riscv``的调用进行区分。
  - 虽然这种写法不是事后看来最优美的，但它减少了代码重构的工作量以及重构失误的可能性。

## 三、对所涉工具软件的介绍

根据在线文档的建议，我使用以下工具：

	- Docker 虚拟化技术的一种实现，帮助我配置环境。
	- Git 一个分布式版本控制软件，帮助我管理代码。
	- Flex (fast lexical analyzer generator)一个快速词法分析器生成器，帮助我生成词法分析器。
	- Bison 一个语法分析器生成器，它把上下文无关文法转换成对应的LALR分析器。
	- VSCode 编辑器，内置的各种插件帮助我管理、编写代码。

****

## 基于 Makefile 的 SysY 编译器项目模板

该仓库中存放了一个基于 Makefile 的 SysY 编译器项目的模板, 你可以在该模板的基础上进行进一步的开发.

该仓库中的 C/C++ 代码实现仅作为演示, 不代表你的编译器必须以此方式实现. 如你需要使用该模板, 建议你删掉所有 C/C++ 源文件, 仅保留 `Makefile` 和必要的目录结构, 然后重新开始实现.

该模板仅供不熟悉 Makefile 的同学参考, 在理解基本原理的基础上, 你完全可以不使用模板完成编译器的实现. 如你决定不使用该模板并自行编写 Makefile, 请参考 [“评测平台要求”](#评测平台要求) 部分.

## 使用方法

首先 clone 本仓库:

```sh
git clone https://github.com/pku-minic/sysy-make-template.git
```

在 [compiler-dev](https://github.com/pku-minic/compiler-dev) 环境内, 进入仓库目录后执行 `make` 即可编译得到可执行文件 (默认位于 `build/compiler`):

```sh
cd sysy-make-template
make
```

如在此基础上进行开发, 你需要重新初始化 Git 仓库:

```sh
rm -rf .git
git init
```

然后, 根据情况修改 `Makefile` 中的 `CPP_MODE` 参数. 如果你决定使用 C 语言进行开发, 你应该将其值改为 `0`.

最后, 将自己的编译器的源文件放入 `src` 目录.

## 测试要求

当你提交一个根目录包含 `Makefile` 文件的仓库时, 测试脚本/评测平台会使用如下命令编译你的编译器:

```sh
make DEBUG=0 BUILD_DIR="build目录" LIB_DIR="libkoopa目录" INC_DIR="libkoopa头文件目录" -C "repo目录"
```

你的 `Makefile` 必须依据 `BUILD_DIR` 参数的值, 将生成的可执行文件输出到该路径中, 并命名为 `compiler`.

如需链接 `libkoopa`, 你的 `Makefile` 应当处理 `LIB_DIR` 和 `INC_DIR`.

模板中的 `Makefile` 已经处理了上述内容, 你无需额外关心.
