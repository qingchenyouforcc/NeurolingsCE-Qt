# rapidxml 嵌入式 XML 解析器

本目录是随引擎编译的 RapidXML 头文件集合，供 shijima/parser 解析 actions.xml 和 behaviors.xml。它不是 NeurolingsCE 的业务层，修改时应优先确认是否能在调用方解决。

## 文件说明

| 文件 | 作用 |
|---|---|
| rapidxml.hpp | RapidXML 核心 DOM 节点、属性、文档和解析 flags。 |
| rapidxml_iterators.hpp | XML 节点/属性的迭代器封装。 |
| rapidxml_print.hpp | 将 RapidXML DOM 打印回 XML 的辅助模板。 |
| rapidxml_utils.hpp | 文件读取和 XML 解析辅助工具。 |

## 使用边界

parser 通常先把文件读入可写缓冲，再让 DOM 引用该缓冲；修改内存所有权、编码或解析 flag 时要同时检查 xml_doc.hpp 和 parser.cc。该目录属于 vendored 代码，保留上游许可证和原有风格。
