/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * ubs-io is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef STANDALONE_VIEW_H
#define STANDALONE_VIEW_H

#include <map>
#include "bio_config_instance.h"
#include "cm.h"

namespace ock {
namespace bio {
class StandaloneView {
public:
    using NodeView = std::map<CmNodeId, CmNodeInfo, CmNodeIdCmp>;
    using PtView = std::map<uint16_t, CmPtInfo>;

    static BResult Build(const BioConfig &config, CmNodeId &localNid, NodeView &nodeView, PtView &ptView);
};
}
}

#endif // STANDALONE_VIEW_H
