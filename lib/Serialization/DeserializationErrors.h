//===--- DeserializationErrors.h - Problems in deserialization --*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SERIALIZATION_DESERIALIZATIONERRORS_H
#define SWIFT_SERIALIZATION_DESERIALIZATIONERRORS_H

#include "swift/AST/Identifier.h"
#include "swift/AST/Module.h"
#include "swift/Serialization/ModuleFormat.h"
#include "llvm/Support/Error.h"

namespace swift {
namespace serialization {

class XRefTracePath {
  class PathPiece {
  public:
    enum class Kind {
      Value,
      Type,
      Operator,
      OperatorFilter,
      Accessor,
      Extension,
      GenericParam,
      Unknown
    };

  private:
    Kind kind;
    void *data;

    template <typename T>
    T getDataAs() const {
      return llvm::PointerLikeTypeTraits<T>::getFromVoidPointer(data);
    }

  public:
    template <typename T>
    PathPiece(Kind K, T value)
      : kind(K),
        data(llvm::PointerLikeTypeTraits<T>::getAsVoidPointer(value)) {}

    void print(raw_ostream &os) const {
      switch (kind) {
      case Kind::Value:
        os << getDataAs<Identifier>();
        break;
      case Kind::Type:
        os << "with type " << getDataAs<Type>();
        break;
      case Kind::Extension:
        if (getDataAs<ModuleDecl *>()) {
          os << "in an extension in module '"
             << getDataAs<ModuleDecl *>()->getName()
             << "'";
        } else {
          os << "in an extension in any module";
        }
        break;
      case Kind::Operator:
        os << "operator " << getDataAs<Identifier>();
        break;
      case Kind::OperatorFilter:
        switch (getDataAs<uintptr_t>()) {
        case Infix:
          os << "(infix)";
          break;
        case Prefix:
          os << "(prefix)";
          break;
        case Postfix:
          os << "(postfix)";
          break;
        default:
          os << "(unknown operator filter)";
          break;
        }
        break;
      case Kind::Accessor:
        switch (getDataAs<uintptr_t>()) {
        case Getter:
          os << "(getter)";
          break;
        case Setter:
          os << "(setter)";
          break;
        case MaterializeForSet:
          os << "(materializeForSet)";
          break;
        case Addressor:
          os << "(addressor)";
          break;
        case MutableAddressor:
          os << "(mutableAddressor)";
          break;
        case WillSet:
          os << "(willSet)";
          break;
        case DidSet:
          os << "(didSet)";
          break;
        default:
          os << "(unknown accessor kind)";
          break;
        }
        break;
      case Kind::GenericParam:
        os << "generic param #" << getDataAs<uintptr_t>();
        break;
      case Kind::Unknown:
        os << "unknown xref kind " << getDataAs<uintptr_t>();
        break;
      }
    }
  };

  ModuleDecl &baseM;
  SmallVector<PathPiece, 8> path;

public:
  explicit XRefTracePath(ModuleDecl &M) : baseM(M) {}

  void addValue(Identifier name) {
    path.push_back({ PathPiece::Kind::Value, name });
  }

  void addType(Type ty) {
    path.push_back({ PathPiece::Kind::Type, ty });
  }

  void addOperator(Identifier name) {
    path.push_back({ PathPiece::Kind::Operator, name });
  }

  void addOperatorFilter(uint8_t fixity) {
    path.push_back({ PathPiece::Kind::OperatorFilter,
                     static_cast<uintptr_t>(fixity) });
  }

  void addAccessor(uint8_t kind) {
    path.push_back({ PathPiece::Kind::Accessor,
                     static_cast<uintptr_t>(kind) });
  }

  void addExtension(ModuleDecl *M) {
    path.push_back({ PathPiece::Kind::Extension, M });
  }

  void addGenericParam(uintptr_t index) {
    path.push_back({ PathPiece::Kind::GenericParam, index });
  }

  void addUnknown(uintptr_t kind) {
    path.push_back({ PathPiece::Kind::Unknown, kind });
  }

  void removeLast() {
    path.pop_back();
  }

  void print(raw_ostream &os, StringRef leading = "") const {
    os << "Cross-reference to module '" << baseM.getName() << "'\n";
    for (auto &piece : path) {
      os << leading << "... ";
      piece.print(os);
      os << "\n";
    }
  }
};

class DeclDeserializationError : public llvm::ErrorInfoBase {
  static const char ID;
  void anchor() override;

public:
  enum Kind {
    Normal,
    DesignatedInitializer
  };

protected:
  Kind kind = Normal;

public:
  Kind getKind() const {
    return kind;
  }

  bool isA(const void *const ClassID) const override {
    return ClassID == classID() || ErrorInfoBase::isA(ClassID);
  }

  static const void *classID() { return &ID; }
};

class XRefError : public llvm::ErrorInfo<XRefError, DeclDeserializationError> {
  friend ErrorInfo;
  static const char ID;
  void anchor() override;

  XRefTracePath path;
  const char *message;
public:
  template <size_t N>
  XRefError(const char (&message)[N], XRefTracePath path)
      : path(path), message(message) {}

  void log(raw_ostream &OS) const override {
    OS << message << "\n";
    path.print(OS);
  }

  std::error_code convertToErrorCode() const override {
    return llvm::inconvertibleErrorCode();
  }
};

class OverrideError : public llvm::ErrorInfo<OverrideError,
                                             DeclDeserializationError> {
private:
  friend ErrorInfo;
  static const char ID;
  void anchor() override;

  DeclName name;

public:
  explicit OverrideError(DeclName name, Kind kind = Normal) : name(name) {
    this->kind = kind;
  }

  void log(raw_ostream &OS) const override {
    OS << "could not find '" << name << "' in parent class";
  }

  std::error_code convertToErrorCode() const override {
    return llvm::inconvertibleErrorCode();
  }
};

class TypeError : public llvm::ErrorInfo<TypeError, DeclDeserializationError> {
  friend ErrorInfo;
  static const char ID;
  void anchor() override;

  DeclName name;
  std::unique_ptr<ErrorInfoBase> underlyingReason;
public:
  explicit TypeError(DeclName name, std::unique_ptr<ErrorInfoBase> reason,
                     Kind kind = Normal)
      : name(name), underlyingReason(std::move(reason)) {
    this->kind = kind;
  }

  void log(raw_ostream &OS) const override {
    OS << "could not deserialize type for '" << name << "'";
    if (underlyingReason) {
      OS << ": ";
      underlyingReason->log(OS);
    }
  }

  std::error_code convertToErrorCode() const override {
    return llvm::inconvertibleErrorCode();
  }
};

} // end namespace serialization
} // end namespace swift

#endif
