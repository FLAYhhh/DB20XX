#pragma once
#include "index_indirection.h"
#include "record.h"
namespace fulgurdb {

char *RecordPtr::get_record_payload() {
    return ptr_->get_record_payload();
}

}
