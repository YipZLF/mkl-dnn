/*******************************************************************************
* Copyright 2019 Intel Corporation
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
*******************************************************************************/

#ifndef OCL_REF_ELTWISE_HPP
#define OCL_REF_ELTWISE_HPP

#include "common/c_types_map.hpp"
#include "ocl/cl_engine.hpp"
#include "ocl/ocl_eltwise_pd.hpp"
#include "ocl/ocl_stream.hpp"
#include "ocl/ocl_utils.hpp"

extern const char *ref_eltwise_kernel;

namespace mkldnn {
namespace impl {
namespace ocl {

struct ref_eltwise_fwd_t : public primitive_t {
    struct pd_t : public ocl_eltwise_fwd_pd_t {
        using ocl_eltwise_fwd_pd_t::ocl_eltwise_fwd_pd_t;

        DECLARE_COMMON_PD_T("ocl:ref:any", ref_eltwise_fwd_t);

        status_t init() {
            auto *cl_engine = utils::downcast<cl_engine_t *>(engine());

            bool ok = true
                    && utils::one_of(desc()->prop_kind,
                               prop_kind::forward_training,
                               prop_kind::forward_inference)
                    && utils::one_of(desc()->alg_kind, alg_kind::eltwise_relu,
                               alg_kind::eltwise_linear,
                               alg_kind::eltwise_bounded_relu,
                               alg_kind::eltwise_soft_relu,
                               alg_kind::eltwise_logistic)
                    && utils::one_of(desc()->data_desc.data_type,
                               data_type::f32, data_type::f16)
                    && memory_desc_wrapper(desc()->data_desc).is_dense()
                    && attr()->has_default_values()
                    && IMPLICATION(
                               desc()->data_desc.data_type == data_type::f16,
                               cl_engine->mayiuse(cl_device_ext_t::khr_fp16));
            if (!ok)
                return status::unimplemented;

            return status::success;
        }
    };

    virtual status_t init() override {
        auto jit = ocl_jit_t(ref_eltwise_kernel);

        jit.set_data_type(pd()->desc()->data_desc.data_type);
        jit.define_int("RELU", alg_kind::eltwise_relu);
        jit.define_int("LINEAR", alg_kind::eltwise_linear);
        jit.define_int("BOUNDED_RELU", alg_kind::eltwise_bounded_relu);
        jit.define_int("SOFT_RELU", alg_kind::eltwise_soft_relu);
        jit.define_int("LOGISTIC", alg_kind::eltwise_logistic);
        jit.define_int("ALG_KIND", pd()->desc()->alg_kind);

        status_t status = jit.build(engine());
        if (status != status::success)
            return status;

        kernel_ = jit.get_kernel("ref_eltwise_fwd");
        return status::success;
    }

    ref_eltwise_fwd_t(const pd_t *apd) : primitive_t(apd) {}

    virtual status_t execute(const exec_ctx_t &ctx) const override {
        return execute_forward_dense(ctx);
    }

private:
    status_t execute_forward_dense(const exec_ctx_t &ctx) const;
    const pd_t *pd() const { return (const pd_t *)primitive_t::pd(); }
    ocl_kernel_t kernel_;
};

struct ref_eltwise_bwd_t : public primitive_t {
    struct pd_t : public ocl_eltwise_bwd_pd_t {
        pd_t(engine_t *engine, const eltwise_desc_t *adesc,
                const primitive_attr_t *attr,
                const eltwise_fwd_pd_t *hint_fwd_pd)
            : ocl_eltwise_bwd_pd_t(engine, adesc, attr, hint_fwd_pd) {}

        DECLARE_COMMON_PD_T("ocl:ref:any", ref_eltwise_bwd_t);

        status_t init() {
            using namespace prop_kind;
            using namespace utils;
            assert(engine()->kind() == engine_kind::gpu);

            memory_desc_wrapper data_mdw(desc()->data_desc);
            memory_desc_wrapper diff_data_mdw(desc()->diff_data_desc);

            bool ok = true
                    && desc()->prop_kind == backward_data
                    && utils::one_of(desc()->alg_kind, alg_kind::eltwise_relu,
                               alg_kind::eltwise_linear,
                               alg_kind::eltwise_bounded_relu,
                               alg_kind::eltwise_soft_relu,
                               alg_kind::eltwise_logistic)
                    && utils::one_of(desc()->data_desc.data_type,
                               data_type::f32, data_type::f16)
                    && data_mdw.is_dense() && data_mdw == diff_data_mdw
                    && attr()->has_default_values();
            if (!ok)
                return status::unimplemented;

            return status::success;
        }

        bool use_dense_;
    };

    status_t init() override {
        auto jit = ocl_jit_t(ref_eltwise_kernel);

        jit.set_data_type(pd()->desc()->data_desc.data_type);
        jit.define_int("RELU", alg_kind::eltwise_relu);
        jit.define_int("LINEAR", alg_kind::eltwise_linear);
        jit.define_int("BOUNDED_RELU", alg_kind::eltwise_bounded_relu);
        jit.define_int("SOFT_RELU", alg_kind::eltwise_soft_relu);
        jit.define_int("LOGISTIC", alg_kind::eltwise_logistic);
        jit.define_int("ALG_KIND", pd()->desc()->alg_kind);

        status_t status = jit.build(engine());
        if (status != status::success)
            return status;

        kernel_ = jit.get_kernel("ref_eltwise_bwd");
        if (!kernel_)
            return status::runtime_error;

        return status::success;
    }

    ref_eltwise_bwd_t(const pd_t *apd) : primitive_t(apd) {}

    ~ref_eltwise_bwd_t() {}

    virtual status_t execute(const exec_ctx_t &ctx) const override {
        return execute_backward_dense(ctx);
    }

private:
    status_t execute_backward_dense(const exec_ctx_t &ctx) const;
    const pd_t *pd() const { return (const pd_t *)primitive_t::pd(); }
    ocl_kernel_t kernel_;
};

} // namespace ocl
} // namespace impl
} // namespace mkldnn

#endif
