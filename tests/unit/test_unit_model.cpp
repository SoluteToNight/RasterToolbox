#include <cassert>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/dispatcher/Task.hpp"

int main() {
    rastertoolbox::dispatcher::Task task;
    assert(task.status == rastertoolbox::dispatcher::TaskStatus::Pending);
    task.status = rastertoolbox::dispatcher::TaskStatus::Running;
    task.errorClass = rastertoolbox::common::ErrorClass::TaskError;
    assert(task.status == rastertoolbox::dispatcher::TaskStatus::Running);
    assert(task.errorClass == rastertoolbox::common::ErrorClass::TaskError);
    return 0;
}
