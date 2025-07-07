
#include "common/common_type.h"

#include <string>
#include <unordered_map>

const std::string UNDEFINE_SDTYPE = "undefine";
const std::string INT_SDTYPE = "int";
const std::string LONG_SDTYPE = "long";
const std::string FLOAT_SDTYPE = "float";
const std::string DOUBLE_SDTYPE = "double";
const std::string STRING_SDTYPE = "string";
const std::string STRINGARRAY_SDTYPE = "stringarrry";
const std::string VECTOR_SDTYPE = "vector";

const std::unordered_map<std::string, DType> sdtype_to_dtype = {
    {UNDEFINE_SDTYPE, DType::UNDEFINE},
    {INT_SDTYPE, DType::INT},
    {LONG_SDTYPE, DType::LONG},
    {FLOAT_SDTYPE, DType::FLOAT},
    {DOUBLE_SDTYPE, DType::DOUBLE},
    {STRING_SDTYPE, DType::STRING},
    {STRINGARRAY_SDTYPE, DType::STRINGARRAY},
    {VECTOR_SDTYPE, DType::VECTOR}};

DType SDTypeToDType(const std::string& sdtype) {
  auto it = sdtype_to_dtype.find(sdtype);
  return it != sdtype_to_dtype.end() ? it->second : DType::UNDEFINE;
};

const std::string& DTypeToSDType(DType t) {
  switch (t) {
    case DType::INT:
      return INT_SDTYPE;
    case DType::LONG:
      return LONG_SDTYPE;
    case DType::FLOAT:
      return FLOAT_SDTYPE;
    case DType::DOUBLE:
      return DOUBLE_SDTYPE;
    case DType::STRING:
      return STRING_SDTYPE;
    case DType::STRINGARRAY:
      return STRINGARRAY_SDTYPE;
    case DType::VECTOR:
      return VECTOR_SDTYPE;
    default:
      return UNDEFINE_SDTYPE;
  }
}

bool IsIdDType(DType dtype) {
  if (dtype == DType::LONG || dtype == DType::STRING) {
    return true;
  }
  return false;
}
