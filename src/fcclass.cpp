// FIXME Check for memory leaks and corruptions in numpy-like return types
// FIXME Check if RowMajor order is the right thing to do everywhere
#include <stdio.h>
#include <assert.h>
#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include <pybind11/stl.h>
#include <Eigen/Dense>

#include "activation.hpp"


namespace py = pybind11;
using namespace pybind11::literals;
using namespace std;


typedef vector<size_t> shape_t;
typedef Eigen::Matrix<double,
                      Eigen::Dynamic,
                      Eigen::Dynamic,
                      Eigen::RowMajor> ematrix_t;
typedef Eigen::VectorXd evector_t;


typedef struct NNLayer {
    ActivationFunction activation;
    ematrix_t w;
} NNLayer;


class FcClassifier {
public:
    // A fully connected neural-network classifier.
    // The last layer has a single output unit with sigmoid activation
    FcClassifier(size_t input_units, const shape_t hidden_units)
    : layers(hidden_units.size() + 1)
    {
        const auto n_layers = hidden_units.size() + 1;
        if(n_layers < 1) {
            throw invalid_argument("Number of layers is too small");
        }

        // Create temporary shapes array to unify loop below
        size_t shapes[n_layers + 1];
        shapes[0] = input_units;
        shapes[n_layers] = 1;
        copy(hidden_units.begin(), hidden_units.end(), shapes + 1);

        // Initialize the weights with zeros
        for (size_t n = 0; n < n_layers; ++n) {
            // +1 to accomodate biases
            layers[n].w = ematrix_t::Zero(shapes[n + 1], shapes[n] + 1);
            layers[n].activation = sigmoid;
        }
    }


    size_t input_units() const { return layers[0].w.cols() - 1; }
    size_t hidden_layers() const { return layers.size() - 1; }


    shape_t hidden_units () const
    {
        shape_t result (layers.size() - 1);
        for (size_t i = 0; i < layers.size() - 1; ++i) {
            result[i] = layers[i].w.rows();
        }
        return result;
    }


    // comments
    void init_random(long seed)
    {
        srand(seed);
        for (auto& layer: layers) {
            layer.w = ematrix_t::Random(layer.w.rows(), layer.w.cols());
        }

    }


    vector<ematrix_t> &get_weights() const
    {
        auto result = new vector<ematrix_t>(layers.size());
        for (size_t i = 0; i < layers.size(); ++i) {
            (*result)[i] = layers[i].w;
        }
        return *result;
    }


    void set_weights(const size_t layer, Eigen::Ref<ematrix_t> weight)
    {
        if((weight.rows() != layers[layer].w.rows()) ||
           (weight.cols() != layers[layer].w.cols())) {
            throw invalid_argument("Set weight has wrong shape");
        }
        layers[layer].w = weight;
    }

    // Note that x_in in TensorFlow like with the sample index being the last
    // one
    evector_t predict(const Eigen::Ref<const ematrix_t> x_in) const
    {
        ematrix_t x_current = x_in;
        for (auto const& layer: layers) {
            auto w = layer.w.block(0, 1, layer.w.rows(), layer.w.cols() - 1);
            auto b = layer.w.col(0);
            x_current = ((w * x_current).colwise() + b).unaryExpr(layer.activation.f);
        }

        std::cout << x_current.rows() << " x " << x_current.cols() << std::endl;
        return x_current.row(0);
    }

    vector<ematrix_t> back_propagate(const Eigen::Ref<const evector_t> x,
                                     const double y)
    {
        auto result = new vector<ematrix_t>(layers.size());
        for (size_t i = 0; i < layers.size(); ++i) {
            auto w = layers[i].w;
            (*result)[i] = ematrix_t::Zero(w.rows(), w.cols());
        }
        return *result;
    }

private:
    vector<NNLayer> layers;
};


PYBIND11_PLUGIN(fcclass)
{
    py::module m("fcclass");

    py::class_<FcClassifier>(m, "FcClassifier")
        .def(py::init<size_t, shape_t>(), "input_units"_a, "hidden_units"_a)
        .def_property_readonly("input_units", &FcClassifier::input_units)
        .def_property_readonly("hidden_layers", &FcClassifier::hidden_layers)
        .def_property_readonly("hidden_units", &FcClassifier::hidden_units)
        .def("init_random", &FcClassifier::init_random, "seed"_a=0)
        .def("get_weights", &FcClassifier::get_weights,
             py::return_value_policy::copy)
        .def("set_weights", &FcClassifier::set_weights, "layer"_a, "weight"_a)
        .def("predict", &FcClassifier::predict, "x_in"_a)
        .def("back_propagate", &FcClassifier::back_propagate,
             py::return_value_policy::copy, "x"_a, "y"_a);

    return m.ptr();
}
