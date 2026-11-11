#pragma once
#include "core/model/Section.h"
#include "core/ds/LinkedList.h"

struct Course {
    std::string code;
    std::string name;
    LinkedList<Section> sections;
};
