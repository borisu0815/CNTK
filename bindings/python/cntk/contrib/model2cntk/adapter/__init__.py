# ==============================================================================
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root
# for full license information.
# ==============================================================================

import adapter

from adapter.bvlccaffe import caffeadapter

ADAPTER_DICT = {
    'Caffe': caffeadapter.CaffeAdapter()
}
