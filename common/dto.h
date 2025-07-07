#pragma once


#include <string>
#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"
#include "gamma/c_api/use_gamma.h"
#include "common/common_type.h"
#include <google/protobuf/repeated_field.h>


std::string CheckPbSpace(const common_rpc::Space& pb_space);


std::string CheckPbDoc(const common_rpc::Document& pb_doc);

std::string CheckPbDoc(const common_rpc::Document& pb_doc,
                       const common_rpc::Space& space);

std::string CheckPbIds(const google::protobuf::RepeatedPtrField<std::string>& doc_keys,
                       const common_rpc::Space& space);

std::string PbDocToGammaDoc(const common_rpc::Document& pb_doc, 
                            const common_rpc::Space& space,
                            vearch::Doc& gamma_doc);

common_rpc::Document GammaDocToPbDoc(vearch::Doc& gamma_doc);


std::string GetPbDocKey(const common_rpc::Document& pb_doc, DType id_dtype);

DataType ToGammaDtype(const std::string& data_type);

vearch::TableInfo GetGammaTable(const common_rpc::Space& space);