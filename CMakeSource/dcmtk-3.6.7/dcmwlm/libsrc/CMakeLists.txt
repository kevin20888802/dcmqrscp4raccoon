# create library from source files
DCMTK_ADD_LIBRARY(dcmwlm
  wlds.cc
  wldsfs.cc
  wlfsim.cc
  wlmactmg.cc
)

DCMTK_TARGET_LINK_MODULES(dcmwlm ofstd dcmdata dcmnet)
target_link_libraries(dcmwlm bson_static mongoc_static)