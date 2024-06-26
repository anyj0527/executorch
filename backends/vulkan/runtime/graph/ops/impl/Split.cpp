/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <executorch/backends/vulkan/runtime/graph/ops/OperatorRegistry.h>

#include <executorch/backends/vulkan/runtime/graph/ops/impl/Copy.h>

#include <executorch/backends/vulkan/runtime/graph/ops/impl/utils/DimUtils.h>
#include <executorch/backends/vulkan/runtime/graph/ops/impl/utils/KernelUtils.h>
#include <executorch/backends/vulkan/runtime/graph/ops/impl/utils/TensorUtils.h>
#include <executorch/backends/vulkan/runtime/graph/ops/utils/ShaderNameUtils.h>

namespace vkcompute {

void add_split_with_sizes_default_node(
    ComputeGraph& graph,
    ValueRef in,
    const std::vector<int64_t>& split_sizes,
    int64_t dim,
    ValueRef out_list_ref) {
  vTensorPtr t_in = graph.get_tensor(in);

  VK_CHECK_COND(check_memory_layout_is(*t_in, api::kChannelsPacked));

  ValueListPtr out_list = graph.get_value_list(out_list_ref);

  NchwDim nchw_dim = normalize_to_nchw_dim(*t_in, dim);

  VK_CHECK_COND(out_list->size() == split_sizes.size());

  for (int split_idx = 0; split_idx < split_sizes.size(); split_idx++) {
    int64_t split_size = split_sizes[split_idx];
    ValueRef out_ref = (*out_list)[split_idx];

    vTensorPtr t_out = graph.get_tensor(out_ref);
    VK_CHECK_COND(check_memory_layout_is(*t_out, api::kChannelsPacked));
    VK_CHECK_COND(dim_at(*t_out, nchw_dim) == split_size);
  }

  if (nchw_dim == DimWidth) {
    api::utils::ivec3 src_offset = api::utils::make_ivec3({0, 0, 0}, false);
    api::utils::ivec3 dst_offset = api::utils::make_ivec3({0, 0, 0}, false);

    for (ValueRef out_ref : *out_list) {
      // Doesn't need to use split_size since we have already verified that the
      // output tensor's size matches with the split_size.
      vTensorPtr t_out = graph.get_tensor(out_ref);
      api::utils::ivec3 range = t_out->texture_limits();
      add_copy_offset_node(graph, in, range, src_offset, dst_offset, out_ref);

      src_offset.data[0] += range.data[0];
    }
  } else if (nchw_dim == DimHeight) {
    api::utils::ivec3 src_offset = api::utils::make_ivec3({0, 0, 0}, false);
    api::utils::ivec3 dst_offset = api::utils::make_ivec3({0, 0, 0}, false);

    for (ValueRef out_ref : *out_list) {
      vTensorPtr t_out = graph.get_tensor(out_ref);
      api::utils::ivec3 range = t_out->texture_limits();
      add_copy_offset_node(graph, in, range, src_offset, dst_offset, out_ref);

      src_offset.data[1] += range.data[1];
    }
  } else if (nchw_dim == DimBatch) {
    api::utils::ivec3 src_offset = api::utils::make_ivec3({0, 0, 0}, false);
    api::utils::ivec3 dst_offset = api::utils::make_ivec3({0, 0, 0}, false);

    for (ValueRef out_ref : *out_list) {
      vTensorPtr t_out = graph.get_tensor(out_ref);
      api::utils::ivec3 range = t_out->texture_limits();
      add_copy_offset_node(graph, in, range, src_offset, dst_offset, out_ref);

      src_offset.data[2] += range.data[2];
    }
  } else if (nchw_dim == DimChannel) {
    int32_t src_offset = 0;
    int32_t dst_offset = 0;

    for (ValueRef out_ref : *out_list) {
      vTensorPtr t_out = graph.get_tensor(out_ref);
      int32_t range = dim_at<Dim4D::Channel>(t_out->sizes());
      add_copy_channel_offset_node(
          graph, in, range, src_offset, dst_offset, out_ref);
      src_offset += range;
    }

  } else {
    VK_THROW("not ipmlemented");
  }
}

void add_split_with_sizes_default_node(
    ComputeGraph& graph,
    ValueRef in,
    ValueRef split_sizes_ref,
    ValueRef dim_ref,
    ValueRef out) {
  int64_t dim = graph.extract_scalar<int64_t>(dim_ref);
  std::vector<int64_t> split_sizes = *(graph.get_int_list(split_sizes_ref));

  add_split_with_sizes_default_node(graph, in, split_sizes, dim, out);
}

void split_with_sizes_copy_default(
    ComputeGraph& graph,
    const std::vector<ValueRef>& args) {
  add_split_with_sizes_default_node(graph, args[0], args[1], args[2], args[3]);
}

void add_split_tensor_node(
    ComputeGraph& graph,
    ValueRef in,
    ValueRef split_size_ref,
    ValueRef dim_ref,
    ValueRef out) {
  int64_t split_size = graph.extract_scalar<int64_t>(split_size_ref);
  int64_t dim = graph.extract_scalar<int64_t>(dim_ref);

  vTensorPtr t_in = graph.get_tensor(in);
  NchwDim nchw_dim = normalize_to_nchw_dim(*t_in, dim);
  int64_t size = dim_at(*t_in, nchw_dim);
  std::vector<int64_t> split_sizes(size / split_size, split_size);

  add_split_with_sizes_default_node(graph, in, split_sizes, dim, out);
}

void split_tensor(ComputeGraph& graph, const std::vector<ValueRef>& args) {
  add_split_tensor_node(graph, args[0], args[1], args[2], args[3]);
}

REGISTER_OPERATORS {
  VK_REGISTER_OP(
      aten.split_with_sizes_copy.default, split_with_sizes_copy_default);
  VK_REGISTER_OP(aten.split.Tensor, split_tensor);
}

} // namespace vkcompute
