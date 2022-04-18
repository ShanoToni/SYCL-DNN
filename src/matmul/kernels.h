/*
 * Copyright Codeplay Software Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SYCLDNN_SRC_MATMUL_KERNELS_H_
#define SYCLDNN_SRC_MATMUL_KERNELS_H_

#include "sycldnn/accessor_types.h"
#include "sycldnn/status.h"

#include "src/matmul/blocks.h"

namespace sycldnn {
namespace matmul {
template <typename T, typename Index, bool TransposeLHS, bool TransposeRHS,
          int RowTile, int AccTile, int ColTile, bool CheckBounds>
struct MatmulKernel {
  MatmulKernel(ReadAccessor<T const> const& lhs,
               ReadAccessor<T const> const& rhs,
               ReadWriteAccessor<T> const& output, Index batches, Index m,
               Index k, Index n, T beta)
      : lhs_{lhs},
        rhs_{rhs},
        output_{output},
        batches_{batches},
        m_{m},
        k_{k},
        n_{n},
        beta_{beta} {}

  void SNN_ALWAYS_INLINE operator()(cl::sycl::nd_item<3> item) const {
    Index batch = item.get_global_id(0);
    Index row = item.get_global_id(1) * RowTile;
    Index col = item.get_global_id(2) * ColTile;

    if (row < m_ && col < n_) {
      auto lhs_ptr = lhs_.get_pointer() + batch * m_ * k_;
      auto rhs_ptr = rhs_.get_pointer() + batch * k_ * n_;
      auto out_ptr = output_.get_pointer() + batch * m_ * n_;

      auto const lhs_ld = TransposeLHS ? m_ : k_;
      auto const lhs_step = (TransposeLHS ? m_ : 1) * AccTile;
      auto const rhs_ld = TransposeRHS ? k_ : n_;
      auto const rhs_step = (TransposeRHS ? 1 : n_) * AccTile;
      auto const out_ld = n_;

      lhs_ptr += (TransposeLHS ? row : k_ * row);
      rhs_ptr += (TransposeRHS ? col * k_ : col);
      out_ptr += out_ld * row + col;

      std::array<bool, RowTile> valid_row;
      for (int i = 0; i < RowTile; ++i) {
        valid_row[i] = row + i < m_;
      }
      std::array<bool, ColTile> valid_col;
      for (int i = 0; i < ColTile; ++i) {
        valid_col[i] = col + i < n_;
      }
      // The boundary of the row/col in matmul are important
      // if the last last element of the block is true, it guarantees
      // that the block is internal and no need for boundaty check
      bool const internal_row_block = valid_row[RowTile - 1];
      bool const internal_col_block = valid_col[ColTile - 1];

      auto out_block = VectorBlock<T, RowTile, ColTile>{};
      if (beta_ != static_cast<T>(0)) {
        // Convert out_ptr from multi_ptr<T> to multi_ptr<T const>
        auto const_out_ptr =
            cl::sycl::multi_ptr<T const,
                                cl::sycl::access::address_space::global_space>{
                out_ptr.get()};

        out_block = load_block<RowTile, ColTile>(const_out_ptr, n_, valid_row,
                                                 valid_col);
        scalar_multiply(out_block, beta_);
      }
      Index acc_idx = 0;

      if (!CheckBounds || (internal_row_block && internal_col_block)) {
        for (; acc_idx < k_ - AccTile + 1; acc_idx += AccTile) {
          auto lhs_block =
              load<RowTile, AccTile, TransposeLHS>(lhs_ptr, lhs_ld);
          auto rhs_block =
              load<AccTile, ColTile, TransposeRHS>(rhs_ptr, rhs_ld);
          block_mmacc(lhs_block, rhs_block, out_block);
          lhs_ptr += lhs_step;
          rhs_ptr += rhs_step;
        }
      }

      if (CheckBounds) {
        auto accumulate_block =
            [&](std::array<bool, AccTile> const& valid_acc) {
              auto lhs_block = load<RowTile, AccTile, TransposeLHS>(
                  lhs_ptr, lhs_ld, valid_row, valid_acc);
              auto rhs_block = load<AccTile, ColTile, TransposeRHS>(
                  rhs_ptr, rhs_ld, valid_acc, valid_col);
              block_mmacc(lhs_block, rhs_block, out_block);
              lhs_ptr += lhs_step;
              rhs_ptr += rhs_step;
            };
        for (; acc_idx < k_ - AccTile + 1; acc_idx += AccTile) {
          std::array<bool, AccTile> valid_acc;
          for (int i = 0; i < AccTile; ++i) {
            valid_acc[i] = true;
          }
          accumulate_block(valid_acc);
        }
        if (acc_idx < k_) {
          std::array<bool, AccTile> valid_acc;
          for (int i = 0; i < AccTile; ++i) {
            valid_acc[i] = acc_idx + i < k_;
          }
          accumulate_block(valid_acc);
        }
      }

      (!CheckBounds || (internal_row_block && internal_col_block))
          ? store_block<RowTile, ColTile>(out_block, out_ptr, out_ld)
          : store_block<RowTile, ColTile>(out_block, out_ptr, out_ld, valid_row,
                                          valid_col);
    }
  }

 private:
  ReadAccessor<T const> lhs_;
  ReadAccessor<T const> rhs_;
  ReadWriteAccessor<T> output_;
  Index const batches_;
  Index const m_;
  Index const k_;
  Index const n_;
  T const beta_;
};

}  // namespace matmul
}  // namespace sycldnn
#endif  // SYCLDNN_SRC_MATMUL_KERNELS_H_
