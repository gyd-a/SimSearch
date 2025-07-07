
#include <sstream>
#include <string>
#include <unordered_map>

#include "gamma/c_api/use_gamma.h"
#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"
#include "common/dto.h"
#include "utils/log.h"
#include "utils/numeric_util.h"


std::string CheckPbSpace(const common_rpc::Space& pb_space) {
  return "";
};


std::string CheckPbDoc(const common_rpc::Document& pb_doc) {
  if (pb_doc.ints_size() == 0 && pb_doc.longs_size() == 0 && pb_doc.floats_size() == 0 &&
      pb_doc.doubles_size() == 0 && pb_doc.strings_size() == 0 &&
      pb_doc.string_arrays_size() == 0 && pb_doc.vectors_size() == 0) {
    return "Document is empty";
  }
  return "";
};

std::string CheckPbDoc(const common_rpc::Document& pb_doc,
                       const common_rpc::Space& space) {
  if (pb_doc._id().empty()) {
    return "Document _id is empty";
  }
  if (space._id_type() == LONG_SDTYPE) {
    int64_t id_value = 0;
    auto status = StrToInt64(pb_doc._id(), id_value);
    if (status.size() > 0) {
      LOG(ERROR) << "request Document _id:" << pb_doc._id() << " StrToInt64 failed: " << status;
      return status;
    }
  }
  std::ostringstream oss;
  for (const auto& field : space.fields()) {
    const std::string& name = field.name();
    const std::string& type = field.type();
    bool found = false;
    if (type == INT_SDTYPE) {
      if (pb_doc.ints().count(name)) {
        found = true;
      }
    } else if (type == LONG_SDTYPE) {
      if (pb_doc.longs().count(name)) {
        found = true;
      }
    } else if (type == FLOAT_SDTYPE) {
      if (pb_doc.floats().count(name)) {
        found = true;
      }
    } else if (type == DOUBLE_SDTYPE) {
      if (pb_doc.doubles().count(name)) {
        found = true;
      }
    } else if (type == STRING_SDTYPE) {
      if (pb_doc.strings().count(name)) {
        found = true;
      }
    } else if (type == STRINGARRAY_SDTYPE) {
      if (pb_doc.string_arrays().count(name)) {
        found = true;
      }
    } else if (type == VECTOR_SDTYPE) {
      auto it = pb_doc.vectors().find(name);
      if (it != pb_doc.vectors().end()) {
        // 可选：校验维度
        if (field.has_dimension() && field.dimension() != it->second.vector_size()) {
          oss << "Field [" << name << "] vector dimension mismatch: expected "
              << field.dimension() << ", got " << it->second.vector_size() << "; ";
          continue;
        } else {
          found = true;
        }
      }
    } else {
      oss << "Unknown type for field [" << name << "]: " << type << "; ";
      continue;;
    }
    if (!found) {
      oss << "Missing field in document: name = " << name << ", type = " << type << "; ";
    }
  }
  return oss.str();  // 返回空字符串表示无错误
}

std::string PbDocToGammaDoc(const common_rpc::Document& pb_doc, const common_rpc::Space& space,
                            vearch::Doc& gamma_doc) {
  std::ostringstream oss;
  if (space._id_type() == LONG_SDTYPE) {
    int64_t id_value = 0;
    std::string status = StrToInt64(pb_doc._id(), id_value);
    if (status.size() > 0) {
      oss << "request Document _id:" << pb_doc._id() << " StrToInt64 failed: " << status;
      LOG(ERROR) << oss.str();
      return oss.str();
    }
    std::string _id_str((char*)&id_value, sizeof(id_value));
    gamma_doc.AddField(vearch::Field{_ID, _id_str, DataType::LONG});
  } else if (space._id_type() == LONG_SDTYPE) {
    gamma_doc.AddField(vearch::Field{_ID, pb_doc._id(), DataType::STRING});
  } else {
    oss << "Unsupported _id type: " << space._id_type();
    LOG(ERROR) << oss.str();
    return oss.str();
  }

  for (const auto& [key, value] : pb_doc.ints()) {
    std::string val((char*)&value, sizeof(value));
    gamma_doc.AddField(vearch::Field{key, val, DataType::INT});
  }
  for (const auto& [key, value] : pb_doc.longs()) {
    std::string val((char*)&value, sizeof(value));
    gamma_doc.AddField(vearch::Field{key, val, DataType::LONG});
  }
  for (const auto& [key, value] : pb_doc.floats()) {
    std::string val((char*)&value, sizeof(value));
    gamma_doc.AddField(vearch::Field{key, val, DataType::FLOAT});
  }
  for (const auto& [key, value] : pb_doc.doubles()) {
    std::string val((char*)&value, sizeof(value));
    gamma_doc.AddField(vearch::Field{key, val, DataType::DOUBLE});
  }
  for (const auto& [key, value] : pb_doc.strings()) {
    gamma_doc.AddField(vearch::Field{key, value, DataType::STRING});
  }
  for (const auto& [key, field] : pb_doc.string_arrays()) {
    vearch::Field gamma_field;
    gamma_field.name = key;
    gamma_field.datatype = DataType::STRINGARRAY;
    for (const auto& val : field.array()) {
      gamma_field.value += val + ",";
    }
    if (!gamma_field.value.empty()) {
      gamma_field.value.pop_back();  // Remove the last comma
    }
    gamma_doc.AddField(std::move(gamma_field));
  }
  for (const auto& [key, field] : pb_doc.vectors()) {
    vearch::Field gamma_field;
    gamma_field.name = key;
    gamma_field.datatype = DataType::VECTOR;
    int bytes_size = field.vector().size() * sizeof(float);
    gamma_field.value.resize(bytes_size);
    if (field.vector().size() > 0) {
      memcpy(gamma_field.value.data(), field.vector().data(), bytes_size);
    }
    gamma_doc.AddField(std::move(gamma_field));
  }
  return "";
}

common_rpc::Document GammaDocToPbDoc(vearch::Doc& gamma_doc) {
  common_rpc::Document pb_doc;
  const auto& table_fields = gamma_doc.TableFields();
  for (const auto& [name, field] : table_fields) {
    // TODO:
    if (field.datatype == DataType::INT) {
      int value = *(int*)(field.value.c_str());
      (*pb_doc.mutable_ints())[name] = value;
    } else if (field.datatype == DataType::LONG) {
      int64_t value = *(int64_t*)(field.value.c_str());
      (*pb_doc.mutable_longs())[name] = value;
    } else if (field.datatype == DataType::FLOAT) {
      float value = *(float*)(field.value.c_str());
      (*pb_doc.mutable_floats())[name] = value;
    } else if (field.datatype == DataType::DOUBLE) {
      double value = *(double*)(field.value.c_str());
      (*pb_doc.mutable_doubles())[name] = value;
    } else if (field.datatype == DataType::STRING) {
      (*pb_doc.mutable_strings())[name] = field.value;
    } else if (field.datatype == DataType::STRINGARRAY) {
      auto* string_array_map = pb_doc.mutable_string_arrays();
      common_rpc::StrArrayField& str_array_field = (*string_array_map)[name];
      std::istringstream iss(field.value);
      std::string item;
      while (std::getline(iss, item, ',')) {
        str_array_field.add_array(item);
      }
    }
  }
  const auto& vector_fields = gamma_doc.VectorFields();
  for (const auto& [name, field] : table_fields) {
    auto* vec_field = &(*pb_doc.mutable_vectors())[name];
    auto* repeated_vec = vec_field->mutable_vector();
    size_t size = field.value.size() / sizeof(float);
    repeated_vec->Reserve(size);  // 预留空间，避免多次realloc
    for (int i = 0; i < size; ++i) {
      float val = *(float*)(field.value.c_str() + i * sizeof(float));
      repeated_vec->Add(val);
    }
  }
  return pb_doc;
}


std::string GetPbDocKey(const common_rpc::Document& pb_doc,
                        DType id_dtype){
  if (id_dtype == DType::LONG) {
    const auto& longs_map = pb_doc.longs();
    auto it = longs_map.find(_ID);
    if (it != longs_map.end()) {
      int64_t value = it->second;
      return std::to_string(value);
    }
  } else if (id_dtype == DType::STRING) {
    const auto& strings_map = pb_doc.strings();
    auto it = strings_map.find(_ID);
    if (it != strings_map.end()) {
      return it->second;
    }
  }
  return "";
}


DataType ToGammaDtype(const std::string& data_type) {
  if (data_type == INT_SDTYPE) {
    return DataType::INT;
  } else if (data_type == FLOAT_SDTYPE) {
    return DataType::FLOAT;
  } else if (data_type == DOUBLE_SDTYPE) {
    return DataType::DOUBLE;
  } else if (data_type == STRING_SDTYPE) {
    return DataType::STRING;
  } else if (data_type == VECTOR_SDTYPE) {
    return DataType::VECTOR;
  }
  return DataType::INT;  // Default case for unknown types
}

std::string CheckPbIds(const google::protobuf::RepeatedPtrField<std::string>& doc_keys,
                       const common_rpc::Space& space) {
  std::set<std::string> st;
  std::ostringstream oss;
  for (auto& key : doc_keys) {
    if (key.empty()) {
      return "Document key is empty";
    }
    if (space._id_type() == LONG_SDTYPE) {
      int64_t id_value = 0;
      std::string status = StrToInt64(key, id_value);
      if (status.size() > 0) {
        oss << "_id:" << key << " StrToInt64 failed: " << status;
        LOG(ERROR) << oss.str();
        return oss.str();
      }

    }
    if (st.count(key) > 0) {
      oss << "Duplicate document key: " << key;
      LOG(ERROR) << oss.str();
      return oss.str();
    }
    st.insert(key);
  }
  return "";
}


vearch::TableInfo GetGammaTable(const common_rpc::Space& space) {
  vearch::TableInfo table_info;
  std::string space_name = space.space_name();
  table_info.SetName(space_name);
  for (const auto& field : space.fields()) {
    auto file_type = field.type();
    if (field.type() == "vector") {
      vearch::VectorInfo vector_info;
      vector_info.name = field.name();
      vector_info.data_type = ToGammaDtype(file_type);
      vector_info.is_index = true;
      vector_info.dimension = field.dimension();
      vector_info.store_param = field.index().params();
      auto index_type = field.index().type();
      auto index_params = field.index().params();
      table_info.SetIndexType(index_type);
      table_info.SetIndexParams(index_params);
      table_info.AddVectorInfo(vector_info);
    } else {
      vearch::FieldInfo field_info;
      LOG(DEBUG) << "GetGammaTable, field name: " << field.name()
                << ", type: " << file_type;
      field_info.name = field.name();
      field_info.data_type = ToGammaDtype(file_type);
      field_info.is_index = field.has_index();
      table_info.AddField(field_info);
    }
  }
  return table_info;
}



