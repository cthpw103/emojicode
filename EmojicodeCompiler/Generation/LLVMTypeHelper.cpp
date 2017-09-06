//
//  LLVMTypeHelper.cpp
//  EmojicodeCompiler
//
//  Created by Theo Weidmann on 06/09/2017.
//  Copyright © 2017 Theo Weidmann. All rights reserved.
//

#include <llvm/IR/DerivedTypes.h>
#include "LLVMTypeHelper.hpp"
#include "../Types/TypeDefinition.hpp"
#include "Mangler.hpp"

namespace EmojicodeCompiler {

LLVMTypeHelper::LLVMTypeHelper(llvm::LLVMContext &context) : context_(context) {
    classMetaType_ = llvm::StructType::create(std::vector<llvm::Type *> {
        llvm::Type::getInt64Ty(context_), llvm::Type::getInt8PtrTy(context_)->getPointerTo(),
    }, "classMeta");
    valueTypeMetaType_ = llvm::StructType::create(std::vector<llvm::Type *> {
        llvm::Type::getInt64Ty(context_),
    }, "valueTypeMeta");
    box_ = llvm::StructType::create(std::vector<llvm::Type *> {
        valueTypeMetaType_->getPointerTo(), llvm::ArrayType::get(llvm::Type::getInt8Ty(context_), 32),
    }, "box");

    types_.emplace(Type::noReturn(), llvm::Type::getVoidTy(context_));
    types_.emplace(Type::integer(), llvm::Type::getInt64Ty(context_));
    types_.emplace(Type::symbol(), llvm::Type::getInt32Ty(context_));
    types_.emplace(Type::doubl(), llvm::Type::getDoubleTy(context_));
    types_.emplace(Type::boolean(), llvm::Type::getInt1Ty(context_));
}

llvm::Type* LLVMTypeHelper::createLlvmTypeForTypeDefinition(const Type &type) {
    std::vector<llvm::Type *> types;

    if (type.type() == TypeType::Class) {
        types.emplace_back(classMetaType_->getPointerTo());
    }

    for (auto &ivar : type.typeDefinition()->instanceVariables()) {
        types.emplace_back(llvmTypeFor(ivar.type));
    }

    auto llvmType = llvm::StructType::create(context_, types, mangleTypeName(type));
    types_.emplace(type, llvmType);
    return llvmType;
}

llvm::Type* LLVMTypeHelper::box() const {
    return box_;
}

llvm::Type* LLVMTypeHelper::valueTypeMetaTypePtr() const {
    return valueTypeMetaType_->getPointerTo();
}

llvm::Type* LLVMTypeHelper::llvmTypeFor(Type type) {
    llvm::Type *llvmType = nullptr;

    if (type.meta()) {
        return classMetaType_->getPointerTo();
    }

    // Error is always a simple optional, so we have to catch it before switching below
    if (type.type() == TypeType::Error) {
        std::vector<llvm::Type *> types{ llvm::Type::getInt1Ty(context_), llvmTypeFor(type.genericArguments()[1]) };
        llvmType = llvm::StructType::get(context_, types);
    }

    switch (type.storageType()) {
        case StorageType::Box:
            llvmType = box_;
            break;
        case StorageType::SimpleOptional: {
            type.setOptional(false);
            std::vector<llvm::Type *> types{ llvm::Type::getInt1Ty(context_), llvmTypeFor(type) };
            llvmType = llvm::StructType::get(context_, types);
            break;
        }
        case StorageType::Simple:
            llvmType = getSimpleType(type);
            break;
    }
    return type.isReference() ? llvmType->getPointerTo() : llvmType;
}

llvm::Type* LLVMTypeHelper::getSimpleType(const Type &type) {
    llvm::Type *llvmType = nullptr;
    auto it = types_.find(type);
    if (it != types_.end()) {
        llvmType = it->second;
    }
    else if (type.type() == TypeType::ValueType || type.type() == TypeType::Class) {
        llvmType = createLlvmTypeForTypeDefinition(type);
    }
    else {
        throw std::logic_error("No llvm type could be established.");
        return llvm::Type::getVoidTy(context_);
    }
    if (type.type() == TypeType::Class) {
        llvmType = llvmType->getPointerTo();
    }
    return llvmType;
}

}  // namespace EmojicodeCompiler