//
// clang_parser.cpp - libclang integration for C++ AST parsing
//
// Phase 1: Parse hello world C++ program, dump AST, regenerate C++ code, compile and run
// Purpose: Fast codebase indexing for efficient AI context building
//

#include "codex.h"
#include <clang-c/Index.h>
#include <sstream>
#include <iomanip>
#include <fstream>

//
// SourceLocation implementation
//

std::string SourceLocation::toString() const {
    std::ostringstream oss;
    if (!file.empty()) {
        oss << file << ":" << line << ":" << column;
    } else {
        oss << "<unknown>:" << line << ":" << column;
    }
    return oss.str();
}

std::string SourceLocation::toStringWithLength() const {
    std::ostringstream oss;
    if (!file.empty()) {
        oss << file << ":" << line << ":" << column << " [" << length << " bytes]";
    } else {
        oss << "<unknown>:" << line << ":" << column << " [" << length << " bytes]";
    }
    return oss.str();
}

//
// ClangParser destructor
//

ClangParser::~ClangParser() {
    if (tu) {
        clang_disposeTranslationUnit(tu);
        tu = nullptr;
    }
}

//
// ClangParser::getLocation - Extract source location from cursor with extent/length
//

SourceLocation ClangParser::getLocation(CXCursor cursor) {
    // Get start location
    CXSourceRange extent = clang_getCursorExtent(cursor);
    CXSourceLocation start_loc = clang_getRangeStart(extent);
    CXSourceLocation end_loc = clang_getRangeEnd(extent);

    CXFile file;
    unsigned line, column, offset;
    clang_getSpellingLocation(start_loc, &file, &line, &column, &offset);

    // Calculate length from extent
    unsigned end_offset;
    clang_getSpellingLocation(end_loc, nullptr, nullptr, nullptr, &end_offset);
    unsigned length = end_offset > offset ? end_offset - offset : 0;

    SourceLocation result;
    result.line = line;
    result.column = column;
    result.offset = offset;
    result.length = length;

    if (file) {
        CXString filename = clang_getFileName(file);
        result.file = clang_getCString(filename);
        clang_disposeString(filename);
    }

    return result;
}

//
// ClangParser::getTypeString - Convert CXType to string
//

std::string ClangParser::getTypeString(CXType type) {
    CXString type_spelling = clang_getTypeSpelling(type);
    std::string result = clang_getCString(type_spelling);
    clang_disposeString(type_spelling);
    return result;
}

//
// ClangParser::generateNodeName - Generate unique node name
//

std::string ClangParser::generateNodeName(const std::string& kind) {
    return kind + "_" + std::to_string(node_counter++);
}

//
// ClangParser::convertType - Convert CXType to ClangType node
//

std::shared_ptr<ClangType> ClangParser::convertType(CXType type) {
    std::string type_str = getTypeString(type);
    CXTypeKind kind = type.kind;

    SourceLocation loc;  // Types don't have source locations in Phase 1
    std::string node_name = generateNodeName("type");

    switch (kind) {
        case CXType_Void:
        case CXType_Bool:
        case CXType_Char_U:
        case CXType_UChar:
        case CXType_Char16:
        case CXType_Char32:
        case CXType_UShort:
        case CXType_UInt:
        case CXType_ULong:
        case CXType_ULongLong:
        case CXType_UInt128:
        case CXType_Char_S:
        case CXType_SChar:
        case CXType_WChar:
        case CXType_Short:
        case CXType_Int:
        case CXType_Long:
        case CXType_LongLong:
        case CXType_Int128:
        case CXType_Float:
        case CXType_Double:
        case CXType_LongDouble:
            return std::make_shared<ClangBuiltinType>(node_name, loc, type_str, type_str);

        case CXType_Pointer: {
            auto ptr_type = std::make_shared<ClangPointerType>(node_name, loc, type_str, type_str);
            CXType pointee = clang_getPointeeType(type);
            ptr_type->pointee = convertType(pointee);
            return ptr_type;
        }

        case CXType_LValueReference:
        case CXType_RValueReference: {
            auto ref_type = std::make_shared<ClangReferenceType>(node_name, loc, type_str, type_str);
            CXType referenced = clang_getPointeeType(type);
            ref_type->referenced = convertType(referenced);
            return ref_type;
        }

        case CXType_Record:
            return std::make_shared<ClangRecordType>(node_name, loc, type_str, type_str);

        case CXType_FunctionProto: {
            auto func_type = std::make_shared<ClangFunctionProtoType>(node_name, loc, type_str, type_str);
            CXType return_type = clang_getResultType(type);
            func_type->return_type = convertType(return_type);

            int num_args = clang_getNumArgTypes(type);
            for (int i = 0; i < num_args; ++i) {
                CXType arg_type = clang_getArgType(type, i);
                func_type->param_types.push_back(convertType(arg_type));
            }
            return func_type;
        }

        default:
            // Generic type node for unsupported types
            return std::make_shared<ClangType>(node_name, loc, type_str, type_str);
    }
}

//
// ClangParser::handleDeclaration - Process declaration cursors
//

std::shared_ptr<ClangAstNode> ClangParser::handleDeclaration(CXCursor cursor) {
    CXCursorKind kind = clang_getCursorKind(cursor);
    SourceLocation loc = getLocation(cursor);
    CXString spelling = clang_getCursorSpelling(cursor);
    std::string spell = clang_getCString(spelling);
    clang_disposeString(spelling);

    std::string node_name = generateNodeName("decl");

    switch (kind) {
        case CXCursor_TranslationUnit: {
            return std::make_shared<ClangTranslationUnitDecl>(node_name, loc, spell);
        }

        case CXCursor_FunctionDecl: {
            auto func = std::make_shared<ClangFunctionDecl>(node_name, loc, spell);

            // Get return type
            CXType result_type = clang_getCursorResultType(cursor);
            func->return_type_str = getTypeString(result_type);
            func->type = convertType(result_type);

            // Get parameters
            int num_args = clang_Cursor_getNumArguments(cursor);
            for (int i = 0; i < num_args; ++i) {
                CXCursor arg = clang_Cursor_getArgument(cursor, i);
                CXType arg_type = clang_getCursorType(arg);
                CXString arg_name = clang_getCursorSpelling(arg);

                func->parameters.push_back({
                    getTypeString(arg_type),
                    clang_getCString(arg_name)
                });

                clang_disposeString(arg_name);
            }

            return func;
        }

        case CXCursor_VarDecl: {
            auto var = std::make_shared<ClangVarDecl>(node_name, loc, spell);
            CXType type = clang_getCursorType(cursor);
            var->type_str = getTypeString(type);
            var->type = convertType(type);
            var->var_name = spell;
            return var;
        }

        case CXCursor_ParmDecl: {
            auto parm = std::make_shared<ClangParmDecl>(node_name, loc, spell);
            CXType type = clang_getCursorType(cursor);
            parm->type_str = getTypeString(type);
            parm->type = convertType(type);
            parm->param_name = spell;
            return parm;
        }

        case CXCursor_FieldDecl: {
            auto field = std::make_shared<ClangFieldDecl>(node_name, loc, spell);
            CXType type = clang_getCursorType(cursor);
            field->type_str = getTypeString(type);
            field->type = convertType(type);
            field->field_name = spell;
            return field;
        }

        case CXCursor_ClassDecl: {
            auto cls = std::make_shared<ClangClassDecl>(node_name, loc, spell);
            cls->class_name = spell;
            return cls;
        }

        case CXCursor_StructDecl: {
            auto str = std::make_shared<ClangStructDecl>(node_name, loc, spell);
            str->struct_name = spell;
            return str;
        }

        case CXCursor_EnumDecl: {
            auto enm = std::make_shared<ClangEnumDecl>(node_name, loc, spell);
            enm->enum_name = spell;
            return enm;
        }

        case CXCursor_Namespace: {
            auto ns = std::make_shared<ClangNamespaceDecl>(node_name, loc, spell);
            ns->namespace_name = spell;
            return ns;
        }

        case CXCursor_TypedefDecl: {
            auto td = std::make_shared<ClangTypedefDecl>(node_name, loc, spell);
            td->typedef_name = spell;
            CXType underlying = clang_getTypedefDeclUnderlyingType(cursor);
            td->underlying_type = getTypeString(underlying);
            return td;
        }

        default:
            return nullptr;
    }
}

//
// ClangParser::handleStatement - Process statement cursors
//

std::shared_ptr<ClangAstNode> ClangParser::handleStatement(CXCursor cursor) {
    CXCursorKind kind = clang_getCursorKind(cursor);
    SourceLocation loc = getLocation(cursor);
    CXString spelling = clang_getCursorSpelling(cursor);
    std::string spell = clang_getCString(spelling);
    clang_disposeString(spelling);

    std::string node_name = generateNodeName("stmt");

    switch (kind) {
        case CXCursor_CompoundStmt: {
            return std::make_shared<ClangCompoundStmt>(node_name, loc, spell);
        }

        case CXCursor_IfStmt: {
            return std::make_shared<ClangIfStmt>(node_name, loc, spell);
        }

        case CXCursor_ForStmt: {
            return std::make_shared<ClangForStmt>(node_name, loc, spell);
        }

        case CXCursor_WhileStmt: {
            return std::make_shared<ClangWhileStmt>(node_name, loc, spell);
        }

        case CXCursor_ReturnStmt: {
            return std::make_shared<ClangReturnStmt>(node_name, loc, spell);
        }

        case CXCursor_DeclStmt: {
            return std::make_shared<ClangDeclStmt>(node_name, loc, spell);
        }

        case CXCursor_BreakStmt: {
            return std::make_shared<ClangBreakStmt>(node_name, loc, spell);
        }

        case CXCursor_ContinueStmt: {
            return std::make_shared<ClangContinueStmt>(node_name, loc, spell);
        }

        default: {
            // Expression statements and others
            if (clang_isExpression(kind)) {
                auto expr_stmt = std::make_shared<ClangExprStmt>(node_name, loc, spell);
                return expr_stmt;
            }
            return nullptr;
        }
    }
}

//
// ClangParser::handleExpression - Process expression cursors
//

std::shared_ptr<ClangAstNode> ClangParser::handleExpression(CXCursor cursor) {
    CXCursorKind kind = clang_getCursorKind(cursor);
    SourceLocation loc = getLocation(cursor);
    CXString spelling = clang_getCursorSpelling(cursor);
    std::string spell = clang_getCString(spelling);
    clang_disposeString(spelling);

    std::string node_name = generateNodeName("expr");

    switch (kind) {
        case CXCursor_BinaryOperator: {
            // Get operator from token scanning (simplified for Phase 1)
            auto binop = std::make_shared<ClangBinaryOperator>(node_name, loc, spell, "?");
            return binop;
        }

        case CXCursor_UnaryOperator: {
            auto unop = std::make_shared<ClangUnaryOperator>(node_name, loc, spell, "?", true);
            return unop;
        }

        case CXCursor_CallExpr: {
            return std::make_shared<ClangCallExpr>(node_name, loc, spell);
        }

        case CXCursor_DeclRefExpr: {
            return std::make_shared<ClangDeclRefExpr>(node_name, loc, spell, spell);
        }

        case CXCursor_IntegerLiteral: {
            // TODO: Extract actual integer value in Phase 2
            return std::make_shared<ClangIntegerLiteral>(node_name, loc, spell, 0);
        }

        case CXCursor_StringLiteral: {
            return std::make_shared<ClangStringLiteral>(node_name, loc, spell, spell);
        }

        case CXCursor_MemberRefExpr: {
            return std::make_shared<ClangMemberRefExpr>(node_name, loc, spell, spell, false);
        }

        case CXCursor_ArraySubscriptExpr: {
            return std::make_shared<ClangArraySubscriptExpr>(node_name, loc, spell);
        }

        default:
            return nullptr;
    }
}

//
// ClangParser::visitChildren - Recursive visitor
//

struct VisitorContext {
    ClangParser* parser;
    std::shared_ptr<ClangAstNode> parent_node;
};

void ClangParser::visitChildren(CXCursor cursor, std::shared_ptr<ClangAstNode> parent_node) {
    VisitorContext ctx = {this, parent_node};

    auto visitor = [](CXCursor c, CXCursor parent, CXClientData client_data) -> CXChildVisitResult {
        (void)parent;  // Unused parameter
        auto* ctx = static_cast<VisitorContext*>(client_data);

        // Convert cursor to AST node
        std::shared_ptr<ClangAstNode> child_node = ctx->parser->convertCursor(c);

        if (child_node && ctx->parent_node) {
            // Add to parent's children
            ctx->parent_node->children()[child_node->name] = child_node;

            // Recursively visit children
            ctx->parser->visitChildren(c, child_node);
        }

        return CXChildVisit_Continue;
    };

    clang_visitChildren(cursor, visitor, &ctx);
}

//
// ClangParser::convertCursor - Main cursor-to-VfsNode converter
//

std::shared_ptr<ClangAstNode> ClangParser::convertCursor(CXCursor cursor) {
    CXCursorKind kind = clang_getCursorKind(cursor);

    std::shared_ptr<ClangAstNode> node;

    if (clang_isDeclaration(kind)) {
        node = handleDeclaration(cursor);
    } else if (clang_isStatement(kind)) {
        node = handleStatement(cursor);
    } else if (clang_isExpression(kind)) {
        node = handleExpression(cursor);
    }

    return node;
}

//
// ClangParser::parseFile - Parse a C++ file from disk
//

bool ClangParser::parseFile(const std::string& filepath, const std::string& vfs_target_path) {
    // Create clang index
    CXIndex index = clang_createIndex(0, 0);
    if (!index) {
        return false;
    }

    // Parse translation unit with C++17 flags
    const char* args[] = {"-std=c++17"};
    tu = clang_parseTranslationUnit(
        index,
        filepath.c_str(),
        args, 1,
        nullptr, 0,
        CXTranslationUnit_None
    );

    if (!tu) {
        clang_disposeIndex(index);
        return false;
    }

    // Get root cursor
    CXCursor cursor = clang_getTranslationUnitCursor(tu);

    // Extract node name from target path
    size_t last_slash = vfs_target_path.rfind('/');
    std::string node_name = (last_slash != std::string::npos)
        ? vfs_target_path.substr(last_slash + 1)
        : vfs_target_path;

    // If no node name provided, use "translation_unit" as default
    if (node_name.empty()) {
        node_name = "translation_unit";
    }

    // Convert to AST node
    SourceLocation loc = getLocation(cursor);
    auto root = std::make_shared<ClangTranslationUnitDecl>(
        node_name,
        loc,
        filepath
    );

    // Visit all children
    visitChildren(cursor, root);

    // Create target directory in VFS and add the node
    if (last_slash != std::string::npos) {
        std::string parent_path = vfs_target_path.substr(0, last_slash);
        vfs.mkdir(parent_path, true);
    }

    // Add node to VFS using the proper API
    std::string parent_dir = vfs_target_path.substr(0, last_slash != std::string::npos ? last_slash : 0);
    if (parent_dir.empty()) parent_dir = "/";
    vfs.addNode(parent_dir, root, 0);

    // Cleanup
    clang_disposeTranslationUnit(tu);
    tu = nullptr;
    clang_disposeIndex(index);

    return true;
}

//
// ClangParser::parseString - Parse C++ source from string
//

bool ClangParser::parseString(const std::string& source, const std::string& filename, const std::string& vfs_target_path) {
    // Create clang index
    CXIndex index = clang_createIndex(0, 0);
    if (!index) {
        return false;
    }

    // Create unsaved file
    CXUnsavedFile unsaved_file;
    unsaved_file.Filename = filename.c_str();
    unsaved_file.Contents = source.c_str();
    unsaved_file.Length = source.length();

    // Parse translation unit with C++17 flags
    const char* args[] = {"-std=c++17"};
    tu = clang_parseTranslationUnit(
        index,
        filename.c_str(),
        args, 1,
        &unsaved_file, 1,
        CXTranslationUnit_None
    );

    if (!tu) {
        clang_disposeIndex(index);
        return false;
    }

    // Get root cursor
    CXCursor cursor = clang_getTranslationUnitCursor(tu);

    // Extract node name from target path
    size_t last_slash = vfs_target_path.rfind('/');
    std::string node_name = (last_slash != std::string::npos)
        ? vfs_target_path.substr(last_slash + 1)
        : vfs_target_path;

    // If no node name provided, use "translation_unit" as default
    if (node_name.empty()) {
        node_name = "translation_unit";
    }

    // Convert to AST node
    SourceLocation loc = getLocation(cursor);
    auto root = std::make_shared<ClangTranslationUnitDecl>(
        node_name,
        loc,
        filename
    );

    // Visit all children
    visitChildren(cursor, root);

    // Create target directory in VFS and add the node
    if (last_slash != std::string::npos) {
        std::string parent_path = vfs_target_path.substr(0, last_slash);
        vfs.mkdir(parent_path, true);
    }

    // Add node to VFS using the proper API
    std::string parent_dir = vfs_target_path.substr(0, last_slash != std::string::npos ? last_slash : 0);
    if (parent_dir.empty()) parent_dir = "/";
    vfs.addNode(parent_dir, root, 0);

    // Cleanup
    clang_disposeTranslationUnit(tu);
    tu = nullptr;
    clang_disposeIndex(index);

    return true;
}

//
// Dump methods for all node types
//

std::string ClangType::dump(int indent) const {
    return ind(indent) + "Type: " + type_name + " @ " + location.toStringWithLength();
}

std::string ClangBuiltinType::dump(int indent) const {
    return ind(indent) + "BuiltinType: " + type_name + " @ " + location.toStringWithLength();
}

std::string ClangPointerType::dump(int indent) const {
    std::string result = ind(indent) + "PointerType: " + type_name + " @ " + location.toStringWithLength() + "\n";
    if (pointee) {
        result += pointee->dump(indent + 1);
    }
    return result;
}

std::string ClangReferenceType::dump(int indent) const {
    std::string result = ind(indent) + "ReferenceType: " + type_name + " @ " + location.toStringWithLength() + "\n";
    if (referenced) {
        result += referenced->dump(indent + 1);
    }
    return result;
}

std::string ClangRecordType::dump(int indent) const {
    return ind(indent) + "RecordType: " + type_name + " @ " + location.toStringWithLength();
}

std::string ClangFunctionProtoType::dump(int indent) const {
    std::string result = ind(indent) + "FunctionProtoType: " + type_name + " @ " + location.toStringWithLength() + "\n";
    if (return_type) {
        result += ind(indent + 1) + "ReturnType:\n";
        result += return_type->dump(indent + 2);
    }
    if (!param_types.empty()) {
        result += ind(indent + 1) + "Parameters:\n";
        for (const auto& param : param_types) {
            result += param->dump(indent + 2) + "\n";
        }
    }
    return result;
}

std::string ClangTranslationUnitDecl::dump(int indent) const {
    std::string result = ind(indent) + "TranslationUnit: " + spelling + " @ " + location.toStringWithLength() + "\n";
    for (const auto& [name, child] : ch) {
        if (auto clang_child = std::dynamic_pointer_cast<ClangAstNode>(child)) {
            result += clang_child->dump(indent + 1) + "\n";
        }
    }
    return result;
}

std::string ClangFunctionDecl::dump(int indent) const {
    std::string result = ind(indent) + "FunctionDecl: " + spelling + " " + return_type_str + "(";
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) result += ", ";
        result += parameters[i].first + " " + parameters[i].second;
    }
    result += ") @ " + location.toStringWithLength() + "\n";

    for (const auto& [name, child] : ch) {
        if (auto clang_child = std::dynamic_pointer_cast<ClangAstNode>(child)) {
            result += clang_child->dump(indent + 1) + "\n";
        }
    }
    return result;
}

std::string ClangVarDecl::dump(int indent) const {
    return ind(indent) + "VarDecl: " + type_str + " " + var_name + " @ " + location.toStringWithLength();
}

std::string ClangParmDecl::dump(int indent) const {
    return ind(indent) + "ParmDecl: " + type_str + " " + param_name + " @ " + location.toStringWithLength();
}

std::string ClangFieldDecl::dump(int indent) const {
    return ind(indent) + "FieldDecl: " + type_str + " " + field_name + " @ " + location.toStringWithLength();
}

std::string ClangClassDecl::dump(int indent) const {
    std::string result = ind(indent) + "ClassDecl: " + class_name + " @ " + location.toStringWithLength() + "\n";
    for (const auto& [name, child] : ch) {
        if (auto clang_child = std::dynamic_pointer_cast<ClangAstNode>(child)) {
            result += clang_child->dump(indent + 1) + "\n";
        }
    }
    return result;
}

std::string ClangStructDecl::dump(int indent) const {
    std::string result = ind(indent) + "StructDecl: " + struct_name + " @ " + location.toStringWithLength() + "\n";
    for (const auto& [name, child] : ch) {
        if (auto clang_child = std::dynamic_pointer_cast<ClangAstNode>(child)) {
            result += clang_child->dump(indent + 1) + "\n";
        }
    }
    return result;
}

std::string ClangEnumDecl::dump(int indent) const {
    std::string result = ind(indent) + "EnumDecl: " + enum_name + " @ " + location.toStringWithLength() + "\n";
    for (const auto& [name, value] : enumerators) {
        result += ind(indent + 1) + name + " = " + std::to_string(value) + "\n";
    }
    return result;
}

std::string ClangNamespaceDecl::dump(int indent) const {
    std::string result = ind(indent) + "NamespaceDecl: " + namespace_name + " @ " + location.toStringWithLength() + "\n";
    for (const auto& [name, child] : ch) {
        if (auto clang_child = std::dynamic_pointer_cast<ClangAstNode>(child)) {
            result += clang_child->dump(indent + 1) + "\n";
        }
    }
    return result;
}

std::string ClangTypedefDecl::dump(int indent) const {
    return ind(indent) + "TypedefDecl: " + typedef_name + " = " + underlying_type + " @ " + location.toStringWithLength();
}

std::string ClangCompoundStmt::dump(int indent) const {
    std::string result = ind(indent) + "CompoundStmt @ " + location.toStringWithLength() + "\n";
    for (const auto& [name, child] : ch) {
        if (auto clang_child = std::dynamic_pointer_cast<ClangAstNode>(child)) {
            result += clang_child->dump(indent + 1) + "\n";
        }
    }
    return result;
}

std::string ClangIfStmt::dump(int indent) const {
    return ind(indent) + "IfStmt @ " + location.toStringWithLength();
}

std::string ClangForStmt::dump(int indent) const {
    return ind(indent) + "ForStmt @ " + location.toStringWithLength();
}

std::string ClangWhileStmt::dump(int indent) const {
    return ind(indent) + "WhileStmt @ " + location.toStringWithLength();
}

std::string ClangReturnStmt::dump(int indent) const {
    return ind(indent) + "ReturnStmt @ " + location.toStringWithLength();
}

std::string ClangDeclStmt::dump(int indent) const {
    return ind(indent) + "DeclStmt @ " + location.toStringWithLength();
}

std::string ClangExprStmt::dump(int indent) const {
    return ind(indent) + "ExprStmt @ " + location.toStringWithLength();
}

std::string ClangBreakStmt::dump(int indent) const {
    return ind(indent) + "BreakStmt @ " + location.toStringWithLength();
}

std::string ClangContinueStmt::dump(int indent) const {
    return ind(indent) + "ContinueStmt @ " + location.toStringWithLength();
}

std::string ClangBinaryOperator::dump(int indent) const {
    return ind(indent) + "BinaryOperator: " + opcode + " @ " + location.toStringWithLength();
}

std::string ClangUnaryOperator::dump(int indent) const {
    return ind(indent) + "UnaryOperator: " + opcode + " @ " + location.toStringWithLength();
}

std::string ClangCallExpr::dump(int indent) const {
    return ind(indent) + "CallExpr: " + spelling + " @ " + location.toStringWithLength();
}

std::string ClangDeclRefExpr::dump(int indent) const {
    return ind(indent) + "DeclRefExpr: " + referenced_decl + " @ " + location.toStringWithLength();
}

std::string ClangIntegerLiteral::dump(int indent) const {
    return ind(indent) + "IntegerLiteral: " + std::to_string(value) + " @ " + location.toStringWithLength();
}

std::string ClangStringLiteral::dump(int indent) const {
    return ind(indent) + "StringLiteral: \"" + value + "\" @ " + location.toStringWithLength();
}

std::string ClangMemberRefExpr::dump(int indent) const {
    return ind(indent) + "MemberRefExpr: " + member_name + " @ " + location.toStringWithLength();
}

std::string ClangArraySubscriptExpr::dump(int indent) const {
    return ind(indent) + "ArraySubscriptExpr @ " + location.toStringWithLength();
}

//
// Code generation helpers
//

// Read source file content (cached for efficiency)
static std::map<std::string, std::string> source_file_cache;

static std::string readSourceFile(const std::string& filepath) {
    auto it = source_file_cache.find(filepath);
    if (it != source_file_cache.end()) {
        return it->second;
    }

    std::ifstream f(filepath);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to open source file: " + filepath);
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    source_file_cache[filepath] = content;
    return content;
}

// Extract source code for a node using its location
static std::string extractSourceCode(const SourceLocation& loc) {
    if (loc.file.empty() || loc.length == 0) {
        return "";
    }

    std::string content = readSourceFile(loc.file);
    if (loc.offset + loc.length > content.size()) {
        return "";
    }

    return content.substr(loc.offset, loc.length);
}

// Generate C++ code from AST node tree
std::string generateCppCode(std::shared_ptr<ClangAstNode> node) {
    if (!node) {
        return "";
    }

    // For Phase 1, we use source extraction
    std::string code = extractSourceCode(node->location);
    if (!code.empty()) {
        return code;
    }

    // Fallback: if we can't extract from source, try to generate from children
    std::ostringstream result;

    // For TranslationUnit, concatenate all top-level declarations
    if (auto tu = std::dynamic_pointer_cast<ClangTranslationUnitDecl>(node)) {
        for (const auto& [name, child] : tu->children()) {
            if (auto clang_child = std::dynamic_pointer_cast<ClangAstNode>(child)) {
                result << generateCppCode(clang_child) << "\n";
            }
        }
    }

    return result.str();
}

//
// Shell commands
//

void cmd_parse_file(Vfs& vfs, const std::vector<std::string>& args) {
    if (args.size() < 1) {
        throw std::runtime_error("parse.file: requires <filepath> [vfs-target-path]");
    }

    std::string filepath = args[0];
    std::string vfs_target = args.size() > 1 ? args[1] : "/ast/" + filepath;

    ClangParser parser(vfs);
    bool success = parser.parseFile(filepath, vfs_target);

    if (!success) {
        throw std::runtime_error("parse.file: failed to parse " + filepath);
    }
}

void cmd_parse_dump(Vfs& vfs, const std::vector<std::string>& args) {
    std::string path = args.empty() ? "/ast" : args[0];

    auto node = vfs.resolve(path);
    if (!node) {
        throw std::runtime_error("parse.dump: path not found: " + path);
    }

    if (auto clang_node = std::dynamic_pointer_cast<ClangAstNode>(node)) {
        std::cout << clang_node->dump(0) << std::endl;
    } else {
        throw std::runtime_error("parse.dump: not a clang AST node: " + path);
    }
}

void cmd_parse_generate(Vfs& vfs, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw std::runtime_error("parse.generate: requires <ast-path> <output-path>");
    }

    std::string ast_path = args[0];
    std::string output_path = args[1];

    // Resolve AST node
    auto node = vfs.resolve(ast_path);
    if (!node) {
        throw std::runtime_error("parse.generate: AST path not found: " + ast_path);
    }

    auto clang_node = std::dynamic_pointer_cast<ClangAstNode>(node);
    if (!clang_node) {
        throw std::runtime_error("parse.generate: not a clang AST node: " + ast_path);
    }

    // Generate C++ code
    std::string cpp_code = generateCppCode(clang_node);
    if (cpp_code.empty()) {
        throw std::runtime_error("parse.generate: failed to generate code from AST");
    }

    // Write to VFS
    size_t last_slash = output_path.rfind('/');
    if (last_slash != std::string::npos) {
        std::string parent_path = output_path.substr(0, last_slash);
        vfs.mkdir(parent_path, true);
    }

    std::string filename = (last_slash != std::string::npos)
        ? output_path.substr(last_slash + 1)
        : output_path;
    std::string parent_dir = (last_slash != std::string::npos)
        ? output_path.substr(0, last_slash)
        : "/";

    auto file = std::make_shared<FileNode>(filename, cpp_code);
    vfs.addNode(parent_dir, file, 0);

    std::cout << "Generated " << cpp_code.length() << " bytes of C++ code to " << output_path << std::endl;
}
