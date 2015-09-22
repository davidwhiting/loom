// Copyright (c) 2014, Salesforce.com, Inc.  All rights reserved.
// Copyright (c) 2015, Google, Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// - Neither the name of Salesforce.com nor the names of its contributors
//   may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <loom/product_mixture.hpp>
#include <distributions/assert_close.hpp>

namespace loom
{

template<bool cached>
struct ProductMixture_<cached>::score_value_group_fun
{
    const Features & mixtures;
    const ProductModel::Features & shareds;
    size_t groupid;
    rng_t & rng;

    float score;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            const typename T::Value & value)
    {
        score += mixtures[t][i].score_value_group(
            shareds[t][i],
            groupid,
            value,
            rng);
    }
};

template<>
inline void ProductMixture_<true>::_update_tare_cache (
        const ProductModel & model,
        size_t groupid,
        rng_t & rng)
{
    LOOM_ASSERT1(maintaining_cache, "cache is not being maintained");

    score_value_group_fun fun = {
        features,
        model.features,
        groupid,
        rng,
        0.f};
    const size_t tare_count = model.tares.size();
    if (LOOM_DEBUG_LEVEL >= 1) {
        LOOM_ASSERT_EQ(tare_caches.size(), tare_count);
    }
    for (size_t i = 0; i < tare_count; ++i) {
        fun.score = 0.f;
        read_value(fun, model.schema, features, model.tares[i]);
        tare_caches[i].scores[groupid] = fun.score;
    }
}

template<>
inline void ProductMixture_<false>::_update_tare_cache (
        const ProductModel &,
        size_t,
        rng_t &)
{
}

template<>
inline void ProductMixture_<true>::_add_tare_cache (
        const ProductModel & model,
        rng_t & rng)
{
    for (auto & tare_cache : tare_caches) {
        tare_cache.scores.packed_add();
    }
    _update_tare_cache(model, clustering.counts().size() - 1, rng);
}

template<>
inline void ProductMixture_<false>::_add_tare_cache (
        const ProductModel &,
        rng_t &)
{
    for (auto & tare_cache : tare_caches) {
        tare_cache.counts.packed_add(0);
    }
}

template<>
inline void ProductMixture_<true>::_remove_tare_cache (size_t groupid)
{
    for (auto & tare_cache : tare_caches) {
        tare_cache.scores.packed_remove(groupid);
    }
}

template<>
inline void ProductMixture_<false>::_remove_tare_cache (size_t groupid)
{
    for (auto & tare_cache : tare_caches) {
        if (LOOM_DEBUG_LEVEL >= 2) {
            LOOM_ASSERT_EQ(tare_cache.counts[groupid], 0);
        }
        tare_cache.counts.packed_remove(groupid);
    }
}

template<bool cached>
struct ProductMixture_<cached>::add_group_fun
{
    Features & mixtures;
    rng_t & rng;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            const typename T::Shared & shared)
    {
        mixtures[t][i].add_group(shared, rng);
    }
};

template<bool cached>
struct ProductMixture_<cached>::add_value_fun
{
    Features & mixtures;
    const ProductModel::Features & shareds;
    const size_t groupid;
    rng_t & rng;

    template<class T>
    void operator() (
        T * t,
        size_t i,
        const typename T::Value & value)
    {
        mixtures[t][i].add_value(shareds[t][i], groupid, value, rng);
    }
};

template<bool cached>
void ProductMixture_<cached>::add_value (
        const ProductModel & model,
        size_t groupid,
        const Value & value,
        rng_t & rng)
{
    LOOM_ASSERT1(maintaining_cache, "cache is not being maintained");

    bool add_group = clustering.add_value(model.clustering, groupid);
    add_value_fun fun = {features, model.features, groupid, rng};
    read_value(fun, model.schema, features, value);

    if (LOOM_UNLIKELY(add_group)) {
        add_group_fun fun = {features, rng};
        for_each_feature(fun, model.features);
        id_tracker.add_group();
        validate(model);
    }
}

template<bool cached>
struct ProductMixture_<cached>::remove_group_fun
{
    Features & mixtures;
    const size_t groupid;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            const typename T::Shared & shared)
    {
        mixtures[t][i].remove_group(shared, groupid);
    }
};

template<bool cached>
struct ProductMixture_<cached>::remove_value_fun
{
    Features & mixtures;
    const ProductModel::Features & shareds;
    const size_t groupid;
    rng_t & rng;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            const typename T::Value & value)
    {
        mixtures[t][i].remove_value(shareds[t][i], groupid, value, rng);
    }
};

template<bool cached>
void ProductMixture_<cached>::remove_value (
        const ProductModel & model,
        size_t groupid,
        const Value & value,
        rng_t & rng)
{
    LOOM_ASSERT1(maintaining_cache, "cache is not being maintained");

    bool remove_group = clustering.remove_value(model.clustering, groupid);
    remove_value_fun fun = {features, model.features, groupid, rng};
    read_value(fun, model.schema, features, value);

    if (LOOM_UNLIKELY(remove_group)) {
        remove_group_fun fun = {features, groupid};
        for_each_feature(fun, model.features);
        id_tracker.remove_group(groupid);
        validate(model);
    }
}

template<bool cached>
void ProductMixture_<cached>::add_diff (
        const ProductModel & model,
        size_t groupid,
        const Value::Diff & diff,
        rng_t & rng)
{
    LOOM_ASSERT1(maintaining_cache, "cache is not being maintained");

    bool add_group = clustering.add_value(model.clustering, groupid);
    {
        add_value_fun fun = {features, model.features, groupid, rng};
        for (auto id : diff.tares()) {
            LOOM_ASSERT1(id < model.tares.size(), "bad tare id: " << id);
            read_value(fun, model.schema, features, model.tares[id]);
        }
        read_value(fun, model.schema, features, diff.pos());
    }
    {
        remove_value_fun fun = {features, model.features, groupid, rng};
        read_value(fun, model.schema, features, diff.neg());
    }
    _update_tare_cache(model, groupid, rng);

    if (LOOM_UNLIKELY(add_group)) {
        add_group_fun fun = {features, rng};
        for_each_feature(fun, model.features);
        _add_tare_cache(model, rng);
        id_tracker.add_group();
        validate(model);
    }
}

template<bool cached>
void ProductMixture_<cached>::remove_diff (
        const ProductModel & model,
        size_t groupid,
        const Value::Diff & diff,
        rng_t & rng)
{
    LOOM_ASSERT1(maintaining_cache, "cache is not being maintained");

    bool remove_group = clustering.remove_value(model.clustering, groupid);
    {
        add_value_fun fun = {features, model.features, groupid, rng};
        read_value(fun, model.schema, features, diff.neg());
    }
    {
        remove_value_fun fun = {features, model.features, groupid, rng};
        read_value(fun, model.schema, features, diff.pos());
        for (auto id : diff.tares()) {
            LOOM_ASSERT1(id < model.tares.size(), "bad tare id: " << id);
            read_value(fun, model.schema, features, model.tares[id]);
        }
    }

    if (LOOM_UNLIKELY(remove_group)) {
        remove_group_fun fun = {features, groupid};
        for_each_feature(fun, model.features);
        _remove_tare_cache(groupid);
        id_tracker.remove_group(groupid);
        validate(model);
    } else {
        _update_tare_cache(model, groupid, rng);
    }
}

template<>
void ProductMixture_<false>::add_diff_step_1_of_2 (
        const ProductModel & model,
        size_t groupid,
        const Value::Diff & diff,
        rng_t & rng)
{
    bool add_group = clustering.add_value(model.clustering, groupid);
    for (auto id : diff.tares()) {
        LOOM_ASSERT1(id < model.tares.size(), "bad tare id: " << id);
        auto & counts = tare_caches[id].counts;
        LOOM_ASSERT2(groupid < counts.size(), "invalid tare counts");
        ++counts[groupid];
    }
    {
        add_value_fun fun = {features, model.features, groupid, rng};
        read_value(fun, model.schema, features, diff.pos());
    }
    {
        remove_value_fun fun = {features, model.features, groupid, rng};
        read_value(fun, model.schema, features, diff.neg());
    }

    if (LOOM_UNLIKELY(add_group)) {
        add_group_fun fun = {features, rng};
        for_each_feature(fun, model.features);
        _add_tare_cache(model, rng);
        id_tracker.add_group();
        validate(model);
    }
}

template<bool cached>
struct ProductMixture_<cached>::add_diff_fun
{
    Features & mixtures;
    const ProductModel::Features & shareds;
    const distributions::Packed_<uint32_t> & counts;
    rng_t & rng;

    template<class T>
    void operator() (
        T * t,
        size_t i,
        const typename T::Value & tare)
    {
        if (LOOM_DEBUG_LEVEL >= 2) {
            LOOM_ASSERT_EQ(counts.size(), mixtures[t][i].groups().size());
        }
        const auto & shared = shareds[t][i];
        auto group = mixtures[t][i].groups().begin();
        for (auto count : counts) {
            if (count) {
                group->add_repeated_value(shared, tare, count, rng);
            }
            ++group;
        }
    }
};

template<>
void ProductMixture_<false>::add_diff_step_2_of_2 (
        const ProductModel & model,
        rng_t & rng)
{
    const size_t tare_count = model.tares.size();
    if (LOOM_DEBUG_LEVEL >= 1) {
        LOOM_ASSERT_EQ(tare_caches.size(), tare_count);
    }
    for (size_t i = 0; i < tare_count; ++i) {
        add_diff_fun fun = {
            features,
            model.features,
            tare_caches[i].counts,
            rng};
        read_value(fun, model.schema, features, model.tares[i]);
    }
}

template<>
void ProductMixture_<false>::remove_unobserved_value (
        const ProductModel & model,
        size_t groupid)
{
    bool remove_group = clustering.remove_value(model.clustering, groupid);

    if (LOOM_UNLIKELY(remove_group)) {
        remove_group_fun fun = {features, groupid};
        for_each_feature(fun, model.features);
        _remove_tare_cache(groupid);
        id_tracker.remove_group(groupid);
        validate(model);
    }
}

template<bool cached>
struct ProductMixture_<cached>::score_value_fun
{
    const Features & mixtures;
    const ProductModel::Features & shareds;
    VectorFloat & scores;
    rng_t & rng;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            const typename T::Value & value)
    {
        mixtures[t][i].score_value(shareds[t][i], value, scores, rng);
    }
};

template<>
void ProductMixture_<true>::score_value (
        const ProductModel & model,
        const Value & value,
        VectorFloat & scores,
        rng_t & rng) const
{
    LOOM_ASSERT1(maintaining_cache, "cache is not being maintained");

    scores.resize(clustering.counts().size());
    clustering.score_value(model.clustering, scores);
    score_value_fun fun = {features, model.features, scores, rng};
    read_value(fun, model.schema, features, value);
}

template<>
void ProductMixture_<true>::score_diff (
        const ProductModel & model,
        const Value::Diff & diff,
        VectorFloat & scores,
        rng_t & rng) const
{
    LOOM_ASSERT1(maintaining_cache, "cache is not being maintained");

    const size_t size = clustering.counts().size();
    scores.resize(size);
    clustering.score_value(model.clustering, scores);
    score_value_fun fun = {features, model.features, scores, rng};
    read_value(fun, model.schema, features, diff.pos());
    if (model.schema.total_size(diff.neg())) {
        distributions::vector_negate(size, scores.data());
        read_value(fun, model.schema, features, diff.neg());
        distributions::vector_negate(size, scores.data());
    }
    for (auto id : diff.tares()) {
        LOOM_ASSERT1(id < model.tares.size(), "bad tare id: " << id);
        const auto & tare_scores = tare_caches[id].scores;
        if (LOOM_DEBUG_LEVEL >= 1) {
            LOOM_ASSERT_EQ(tare_scores.size(), size);
        }
        distributions::vector_add(size, scores.data(), tare_scores.data());
    }
}

template<bool cached>
struct ProductMixture_<cached>::score_value_features_fun
{
    const Features & mixtures;
    const ProductModel::Features & shareds;
    VectorFloat ** scores;
    rng_t & rng;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            const typename T::Value & value)
    {
        mixtures[t][i].score_value(shareds[t][i], value, **scores++, rng);
    }
};

template<>
void ProductMixture_<true>::score_value_features (
        const ProductModel & model,
        const Value & value,
        std::vector<VectorFloat *> & feature_scores,
        rng_t & rng) const
{
    if (LOOM_DEBUG_LEVEL >= 1) {
        LOOM_ASSERT1(maintaining_cache, "cache is not being maintained");
        LOOM_ASSERT_EQ(
            feature_scores.size(),
            model.schema.observed_count(value.observed()));
    }

    const size_t group_count = clustering.counts().size();
    for (auto * scores : feature_scores) {
        scores->clear();
        scores->resize(group_count, 0);
    }
    score_value_features_fun fun = {
        features,
        model.features,
        feature_scores.data(),
        rng};
    read_value(fun, model.schema, features, value);
}

template<bool cached>
struct ProductMixture_<cached>::init_feature_cache_fun
{
    const ProductModel::Features & shareds;
    rng_t & rng;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            typename T::template Mixture<cached>::t & mixture)
    {
        mixture.init(shareds[t][i], rng);
    }
};

template<bool cached>
void ProductMixture_<cached>::init_feature_cache (
        const ProductModel & model,
        size_t featureid,
        rng_t & rng)
{
    if (maintaining_cache) {
        init_feature_cache_fun fun = {model.features, rng};
        for_one_feature(fun, features, featureid);
    }
}

template<bool cached>
void ProductMixture_<cached>::init_tare_cache (
        const ProductModel & model,
        rng_t & rng)
{
    if (maintaining_cache) {
        tare_caches.resize(model.tares.size());
        const size_t group_count = clustering.counts().size();
        if (cached) {
            for (size_t i = 0, size = model.tares.size(); i < size; ++i) {
                const Value & tare = model.tares[i];
                auto & scores = tare_caches[i].scores;
                scores.resize(group_count);
                distributions::vector_zero(scores.size(), scores.data());
                score_value_fun fun = {features, model.features, scores, rng};
                read_value(fun, model.schema, features, tare);
            }
        } else {
            for (auto & tare_cache : tare_caches) {
                tare_cache.counts.clear();
                tare_cache.counts.resize(group_count, 0);
            }
        }
    }
}

template<bool cached>
struct ProductMixture_<cached>::score_data_fun
{
    const Features & mixtures;
    rng_t & rng;
    float & score;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            const typename T::Shared & shared)
    {
        score += mixtures[t][i].score_data(shared, rng);
    }
};

template<bool cached>
float ProductMixture_<cached>::score_data (
        const ProductModel & model,
        rng_t & rng) const
{
    float score = clustering.score_data(model.clustering);

    score_data_fun fun = {features, rng, score};
    for_each_feature(fun, model.features);

    return score;
}

template<bool cached>
struct ProductMixture_<cached>::score_feature_fun
{
    const Features & mixtures;
    rng_t & rng;
    float score;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            const typename T::Shared & shared)
    {
        score = mixtures[t][i].score_data(shared, rng);
    }
};

template<bool cached>
float ProductMixture_<cached>::score_feature (
        const ProductModel & model,
        size_t featureid,
        rng_t & rng) const
{
    score_feature_fun fun = {features, rng, NAN};
    for_one_feature(fun, model.features, featureid);
    return fun.score;
}

template<bool cached>
struct ProductMixture_<cached>::sample_fun
{
    const Features & mixtures;
    const ProductModel::Features & shareds;
    const size_t groupid;
    rng_t & rng;

    template<class T>
    typename T::Value operator() (T * t, size_t i)
    {
        return mixtures[t][i].groups(groupid).sample_value(shareds[t][i], rng);
    }
};

template<bool cached>
size_t ProductMixture_<cached>::sample_value (
        const ProductModel & model,
        const VectorFloat & probs,
        Value & value,
        rng_t & rng) const
{
    size_t groupid = distributions::sample_from_probs(rng, probs);
    sample_fun fun = {features, model.features, groupid, rng};
    write_value(fun, model.schema, features, value);
    return groupid;
}

template<bool cached>
struct ProductMixture_<cached>::init_unobserved_fun
{
    size_t group_count;
    const ProductModel::Features & shared_features;
    Features & mixture_features;
    const bool maintaining_cache;
    rng_t & rng;

    template<class T>
    void operator() (T * t)
    {
        const auto & shareds = shared_features[t];
        auto & mixtures = mixture_features[t];

        mixtures.clear();
        for (size_t i = 0; i < shareds.size(); ++i) {
            const auto & shared = shareds[i];
            auto & mixture = mixtures.insert(shareds.index(i));
            mixture.groups().resize(group_count);
            for (auto & group : mixture.groups()) {
                group.init(shared, rng);
            }
            if (maintaining_cache) {
                mixture.init(shared, rng);
            }
        }
    }
};

template<bool cached>
void ProductMixture_<cached>::init_unobserved (
        const ProductModel & model,
        const std::vector<int> & counts,
        rng_t & rng)
{
    clustering.counts() = counts;
    clustering.init(model.clustering);

    init_unobserved_fun fun = {
        counts.size(),
        model.features,
        features,
        maintaining_cache,
        rng};
    for_each_feature_type(fun);

    init_tare_cache(model, rng);
    id_tracker.init(counts.size());

    validate(model);
}

template<bool cached>
struct ProductMixture_<cached>::clear_fun
{
    const ProductModel::Features & shareds;
    Features & mixtures;

    template<class T>
    void operator() (T * t)
    {
        mixtures[t].clear();
        for (auto featureid : shareds[t].index()) {
            mixtures[t].insert(featureid);
        }
    }
};

template<bool cached>
struct ProductMixture_<cached>::load_group_fun
{
    const protobuf::ProductModel::Group & messages;
    protobuf::ModelCounts model_counts;

    template<class T>
    void operator() (
            T * t,
            size_t,
            typename T::template Mixture<cached>::t & mixture)
    {
        auto & groups = mixture.groups();
        groups.resize(groups.size() + 1);
        size_t offset = model_counts[t]++;
        const auto & message = protobuf::Fields<T>::get(messages).Get(offset);
        groups.back().protobuf_load(message);
    }
};

template<bool cached>
void ProductMixture_<cached>::load_step_1_of_3 (
        const ProductModel & model,
        const char * filename,
        size_t empty_group_count)
{
    clear_fun fun = {model.features, features};
    for_each_feature_type(fun);
    auto & counts = clustering.counts();
    counts.clear();
    for (auto & tare_cache : tare_caches) {
        tare_cache.scores.clear();
        tare_cache.counts.clear();
    }

    protobuf::InFile groups(filename);
    protobuf::ProductModel::Group message;
    while (groups.try_read_stream(message)) {
        counts.push_back(message.count());
        load_group_fun fun = {message, protobuf::ModelCounts()};
        for_each_feature(fun, features);
    }

    counts.resize(counts.size() + empty_group_count, 0);
    clustering.init(model.clustering);
    id_tracker.init(counts.size());
}

template<bool cached>
struct ProductMixture_<cached>::init_groups_fun
{
    const ProductModel::Features & shareds;
    const size_t empty_group_count;
    const bool maintaining_cache;
    rng_t & rng;

    template<class T>
    void operator() (
            T * t,
            size_t i,
            typename T::template Mixture<cached>::t & mixture)
    {
        const typename T::Shared & shared = shareds[t][i];
        std::vector<typename T::Group> & groups = mixture.groups();
        const size_t nonempty_group_count = groups.size();
        const size_t group_count = nonempty_group_count + empty_group_count;
        groups.resize(groups.size() + empty_group_count);
        for (size_t i = nonempty_group_count; i < group_count; ++i) {
            groups[i].init(shared, rng);
        }
        if (maintaining_cache) {
            mixture.init(shared, rng);
        }
    }
};

template<bool cached>
void ProductMixture_<cached>::load_step_2_of_3 (
        const ProductModel & model,
        size_t featureid,
        size_t empty_group_count,
        rng_t & rng)
{
    init_groups_fun fun = {
        model.features,
        empty_group_count,
        maintaining_cache,
        rng};
    for_one_feature(fun, features, featureid);
}

template<bool cached>
void ProductMixture_<cached>::load_step_3_of_3 (
        const ProductModel & model,
        rng_t & rng)
{
    init_tare_cache(model, rng);
}

template<bool cached>
struct ProductMixture_<cached>::dump_group_fun
{
    size_t groupid;
    protobuf::ProductModel::Group & message;

    template<class T>
    void operator() (
            T *,
            size_t,
            const typename T::template Mixture<cached>::t & mixture)
    {
        const auto & group = mixture.groups(groupid);
        group.protobuf_dump(* protobuf::Fields<T>::get(message).Add());
    }
};

template<bool cached>
void ProductMixture_<cached>::dump (
        const char * filename,
        const std::vector<uint32_t> & sorted_to_global) const
{
    const size_t group_count = clustering.counts().size();
    LOOM_ASSERT_LT(sorted_to_global.size(), group_count);
    protobuf::OutFile groups_stream(filename);
    protobuf::ProductModel::Group message;
    for (auto global : sorted_to_global) {
        auto packed = id_tracker.global_to_packed(global);
        if (LOOM_DEBUG_LEVEL >= 1) {
            LOOM_ASSERT_LT(packed, group_count);
            LOOM_ASSERT_LT(0, clustering.counts(packed));
        }
        message.set_count(clustering.counts(packed));
        dump_group_fun fun = {packed, message};
        for_each_feature(fun, features);
        groups_stream.write_stream(message);
        message.Clear();
    }
}

template<bool cached>
template<class OtherMixture>
struct ProductMixture_<cached>::move_feature_to_fun
{
    const size_t featureid;
    ProductModel::Features & source_shareds;
    typename OtherMixture::Features & source_mixtures;
    ProductModel::Features & destin_shareds;
    typename OtherMixture::Features & destin_mixtures;

    template<class T>
    void operator() (
            T * t,
            size_t,
            typename T::template Mixture<cached>::t & temp_mixture)
    {
        typedef typename T::Shared Shared;
        Shared & source_shared = source_shareds[t].find(featureid);
        Shared & destin_shared = destin_shareds[t].insert(featureid);
        destin_shared = std::move(source_shared);
        source_shareds[t].remove(featureid);

        source_mixtures[t].remove(featureid);
        auto & destin_mixture = destin_mixtures[t].insert(featureid);
        destin_mixture.groups() = std::move(temp_mixture.groups());
    }
};

template<bool cached>
template<class OtherMixture>
void ProductMixture_<cached>::move_feature_to (
        size_t featureid,
        ProductModel & source_model, OtherMixture & source_mixture,
        ProductModel & destin_model, OtherMixture & destin_mixture)
{
    LOOM_ASSERT1(not maintaining_cache, "cannot maintain cache");
    LOOM_ASSERT1(not source_mixture.maintaining_cache, "cannot maintain cache");
    LOOM_ASSERT1(not destin_mixture.maintaining_cache, "cannot maintain cache");
    if (LOOM_DEBUG_LEVEL >= 1) {
        LOOM_ASSERT_EQ(
            destin_mixture.clustering.counts().size(),
            clustering.counts().size());
    }
    if (LOOM_DEBUG_LEVEL >= 2) {
        LOOM_ASSERT_EQ(
            destin_mixture.clustering.counts(),
            clustering.counts());
    }

    move_feature_to_fun<OtherMixture> fun = {
        featureid,
        source_model.features, source_mixture.features,
        destin_model.features, destin_mixture.features};
    for_one_feature(fun, features, featureid);

    source_model.schema.load(source_model.features);
    destin_model.schema.load(destin_model.features);
}

template<bool cached>
template<bool other_cached>
struct ProductMixture_<cached>::validate_subset_fun
{
    const Features & super_features;
    const typename ProductMixture_<other_cached>::Features & sub_features;
    const size_t group_count;

    template<class T>
    void operator() (T * t)
    {
        const auto & super_feature = super_features[t];
        const auto & sub_feature = sub_features[t];
        LOOM_ASSERT_LE(sub_feature.size(), super_feature.size());
        typename T::Protobuf::Group super_group;
        typename T::Protobuf::Group sub_group;
        for (size_t f = 0; f < sub_feature.size(); ++f) {
            size_t featureid = sub_feature.index(f);
            auto & super_groups = super_feature.find(featureid).groups();
            auto & sub_groups = sub_feature.find(featureid).groups();
            for (size_t g = 0; g < group_count; ++g) {
                super_groups[g].protobuf_dump(super_group);
                sub_groups[g].protobuf_dump(sub_group);
                DIST_ASSERT_CLOSE(super_group, sub_group);
            }
        }
    }
};

template<bool cached>
template<bool other_cached>
void ProductMixture_<cached>::validate_subset (
        const ProductMixture_<other_cached> & other) const
{
    const size_t group_count = clustering.counts().size();
    validate_subset_fun<other_cached> fun = {
        features,
        other.features,
        group_count};
    for_each_feature_type(fun);
}

//----------------------------------------------------------------------------
// explicit template instantiation

template struct ProductMixture_<true>;
template struct ProductMixture_<false>;
template void ProductMixture_<false>::validate_subset (
        const ProductMixture_<true> &) const;
template void ProductMixture_<false>::move_feature_to (
        size_t,
        ProductModel &, ProductMixture_<true> &,
        ProductModel &, ProductMixture_<true> &);

} // namespace loom
