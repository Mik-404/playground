#include "graph_types.hpp"

/**
 * @class Pipeline
 * @brief A class to chain and execute a sequence of processing steps.
 *        Each step is a function that takes an std::any and returns an std::any.
 */
class Pipeline {
private:
    vector<std::function<std::any(std::any&&)>> m_steps;

public:
    Pipeline() = default;

    ~Pipeline() = default;

    Pipeline(const Pipeline& other) = default;

    Pipeline& operator=(const Pipeline& other) = default;

    Pipeline(Pipeline&& other) noexcept = default;

    Pipeline& operator=(Pipeline&& other) noexcept = default;

    /**
     * @brief Adds a step to the pipeline.
     * @tparam Func Type of the callable object (function, lambda, functor).
     * @param func Function to be added.
     * @return Reference to this object to allow method chaining.
     */
    template<typename Func>
    Pipeline& add_step(Func&& func) {
        using ArgType = typename decltype(std::function{func})::argument_type;
        using ReturnType = typename decltype(std::function{func})::result_type;

        m_steps.emplace_back([f = std::forward<Func>(func)](std::any&& arg) -> std::any {  
            ArgType input = std::any_cast<ArgType>(std::move(arg));
            
            if constexpr (std::is_void_v<ReturnType>) {
                f(std::move(input));
                return std::any(); 
            } else {
                return f(std::move(input));
            }

        });
        return *this;
    }

    /**
     * @brief Executes the pipeline.
     * @param initial_value Initial value.
     * @return Final result stored in std::any.
     */
    std::any execute(std::any value) const {

        for (const auto& step : m_steps) {
            value = step(std::move(value));
        }
        return value;
    }
};