#include "algorithm8.hpp"
#include <distributions/random.hpp>
#include <distributions/vector_math.hpp>

namespace loom
{

void Algorithm8::clear ()
{
    schema.clear();
    model.clear();
    kinds.clear();
}

void Algorithm8::model_load (CrossCat &)
{
    TODO("load model");
}

void Algorithm8::mixture_dump (CrossCat &)
{
    TODO("dump mixtures");
}

void Algorithm8::mixture_init_empty (rng_t & rng, size_t kind_count)
{
    LOOM_ASSERT_LT(0, kind_count);
    kinds.resize(kind_count);
    for (auto & kind : kinds) {
        kind.mixture.init_empty(model, rng);
    }
}

void Algorithm8::infer_assignments (
        std::vector<size_t> & featureid_to_kindid,
        size_t iterations,
        rng_t & rng)
{
    LOOM_ASSERT_LT(0, iterations);

    const size_t feature_count = featureid_to_kindid.size();
    const size_t kind_count = kinds.size();
    std::vector<VectorFloat> likelihoods(feature_count);

    #pragma omp parallel for schedule(dynamic, 1)
    for (size_t featureid = 0; featureid < feature_count; ++featureid) {
        VectorFloat & scores = likelihoods[featureid];
        scores.resize(kind_count);
        for (size_t kindid = 0; kindid < feature_count; ++kindid) {
            const auto & mixture = kinds[kindid].mixture;
            scores[kindid] = mixture.score_feature(model, featureid, rng);
        }
        distributions::scores_to_likelihoods(scores);
    }

    TODO("do something with likelihoods");
}

} // namespace loom
