#include "product_model.hpp"

namespace loom
{

void ProductModel::load (
        const protobuf::ProductModel_Shared & message,
        const std::vector<size_t> & featureids)
{
    clear();
    distributions::clustering_load(clustering, message.clustering());

    size_t absolute_pos = 0;

    for (size_t i = 0; i < message.bb_size(); ++i) {
        TODO("load bb models");
    }

    for (size_t i = 0; i < message.dd_size(); ++i) {
        size_t featureid = featureids.at(absolute_pos++);
        size_t dim = message.dd(i).alphas().size();
        LOOM_ASSERT1(dim > 1, "invalid dim: " << dim);
        if (dim < 256) {
            auto & shared = features.dd256.insert(featureid);
            distributions::shared_load(shared, message.dd(i));
        } else {
            LOOM_ERROR("invalid dim: " << dim);
        }
    }

    for (size_t i = 0; i < message.dpd_size(); ++i) {
        auto & shared = features.dpd.insert(featureids.at(absolute_pos++));
        distributions::shared_load(shared, message.dpd(i));
        size_t dim = shared.betas.size();
        LOOM_ASSERT1(dim > 1, "invalid dim: " << dim);
    }

    for (size_t i = 0; i < message.gp_size(); ++i) {
        auto & shared = features.gp.insert(featureids.at(absolute_pos++));
        distributions::shared_load(shared, message.gp(i));
    }

    for (size_t i = 0; i < message.nich_size(); ++i) {
        auto & shared = features.nich.insert(featureids.at(absolute_pos++));
        distributions::shared_load(shared, message.nich(i));
    }

    update_schema();
}

struct ProductModel::dump_fun
{
    const Features & features;
    protobuf::ProductModel_Shared & message;

    template<class T>
    void operator() (T * t)
    {
        for (const auto & shared : features[t]) {
            distributions::shared_dump(
                shared,
                * protobuf::Shareds<T>::get(message).Add());
        }
    }
};

void ProductModel::dump (protobuf::ProductModel_Shared & message) const
{
    distributions::clustering_dump(
        clustering,
        * message.mutable_clustering());

    dump_fun fun = {features, message};
    for_each_feature_type(fun);
}


void ProductModel::update_schema ()
{
    schema.clear();
    schema.counts_size += features.dd256.size();
    schema.counts_size += features.dpd.size();
    schema.counts_size += features.gp.size();
    schema.reals_size += features.nich.size();
}

struct ProductModel::clear_fun
{
    Features & features;

    template<class T>
    void operator() (T * t)
    {
        features[t].clear();
    }
};

void ProductModel::clear ()
{
    schema.clear();

    clear_fun fun = {features};
    for_each_feature_type(fun);
}

struct ProductModel::extend_fun
{
    Features & destin;
    const Features & source;

    template<class T>
    void operator() (T * t)
    {
        destin[t].extend(source[t]);
    }
};

void ProductModel::extend (const ProductModel & other)
{
    schema += other.schema;

    extend_fun fun = {features, other.features};
    for_each_feature_type(fun);
}

} // namespace loom
