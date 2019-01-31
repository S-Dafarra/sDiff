/*
 * Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia
 * Authors: Stefano Dafarra
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 *
 */
#ifndef LEVI_EVALUABLE_H
#define LEVI_EVALUABLE_H

#include <levi/ForwardDeclarations.h>

/**
 * @brief Evaluable class (for matrix type)
 *
 * The evaluable class defines a block which can be evaluated. It can be a matrix or a scalar (double, float,..).
 * User can define his own evaluable by publicly inheriting from this class and overload the evaluate() method.
 * It will be on the user to define which value the evaluable will take.
 * The method getColumnDerivative has to be overloaded in order to specify the derivative.
 *
 * Take a look at the ExpressionComponent documentation to place an evaluable into an expression.
 */
template<typename Matrix>
class levi::Evaluable<Matrix, typename std::enable_if<!std::is_arithmetic<Matrix>::value>::type> {

    /**
     * @brief Name of the evaluable
     */
    std::string m_name;

public:

    /**
     * @brief The storage type used in the evaluable.
     */
    typedef Matrix matrix_type;

    /**
     * @brief The basic type used into the matrix.
     */
    typedef typename Matrix::value_type value_type;

    /**
     * @brief Number of rows known at compile time (or Eigen::Dynamic, i.e. -1, in case of dynamic types)
     */
    static const Eigen::Index rows_at_compile_time = Matrix::RowsAtCompileTime;

    /**
     * @brief Number of cols known at compile time (or Eigen::Dynamic, i.e. -1, in case of dynamic types)
     */
    static const Eigen::Index cols_at_compile_time = Matrix::ColsAtCompileTime;

    /**
     * @brief The matrix type used to store a single row.
     */
    typedef Eigen::Matrix<value_type, 1, cols_at_compile_time> row_type;

    /**
     * @brief The matrix type used to store a single column.
     */
    typedef Eigen::Matrix<value_type, rows_at_compile_time, 1> col_type;

    /**
     * @brief The Evaluable type used to store the column derivative.
     */
    typedef Evaluable<Eigen::Matrix<value_type, rows_at_compile_time, Eigen::Dynamic>> derivative_evaluable;

protected:

    /**
     * @brief The buffer used to store the values of the evaluable.
     */
    Matrix m_evaluationBuffer;

public:

    Evaluable() = delete;

    /**
     * @brief Constructor
     * @param  Name of the evaluable
     *
     * The evaluation buffer (m_evaluationBuffer) is not initialized nor resized.
     */
    Evaluable(const std::string& name)
        : m_name(name)
    { }

    /**
     * @brief Constructor
     * @param rows Number of rows of the evaluable.
     * @param cols Number of columns of the evaluable.
     * @param name Name of the evaluable.
     */
    Evaluable(Eigen::Index rows, Eigen::Index cols, const std::string& name)
        : m_name(name)
        , m_evaluationBuffer(rows, cols)
    {
        m_evaluationBuffer.setZero();
    }

    /**
     * @brief Constructor
     * @param initialValue Initial value of the evaluation buffer.
     * @param name Name of the evaluable.
     */
    Evaluable(const Matrix& initialValue, const std::string& name)
        : m_name(name)
        , m_evaluationBuffer(initialValue)
    { }

    template <typename OtherMatrix>
    Evaluable(const Evaluable<OtherMatrix>& other) = delete;

    template <typename OtherMatrix>
    Evaluable(Evaluable<OtherMatrix>&& other) = delete;

    virtual ~Evaluable() { }

    /**
     * @brief Number of rows of the evaluable
     * @return the number of rows of the evaluable
     */
    Eigen::Index rows() const {
        return m_evaluationBuffer.rows();
    }

    /**
     * @brief Number of cols of the evaluable
     * @return the number of cols of the evaluable
     */
    Eigen::Index cols() const {
        return m_evaluationBuffer.cols();
    }

    /**
     * @brief Resize the evaluation buffer.
     * @param newRows New number of rows
     * @param newCols New number of columns
     *
     * @warning This performs dynamic memory allocation.
     *
     * @Note This method should be called only if the matrix storage type is dynamic.
     */
    void resize(Eigen::Index newRows, Eigen::Index newCols) {
        m_evaluationBuffer.resize(newRows, newCols);
    }

    /**
     * @brief Return the name of the evaluable
     * @return The name of the evaluable
     */
    std::string name() const {
        return m_name;
    }

    /**
     * @brief Evaluate the evaluable
     *
     * User should override this method to define custom evaluable.
     *
     * @return const reference to the evaluation buffer.
     */
    virtual const Matrix& evaluate() = 0;

    /**
     * @brief Get the derivative of a specified column with respect to a specified variable
     * @param column The index with respect to the derivative has to be computed.
     * @param variable The variable with respect to the derivative has to be computed.
     * @return An expression containing an evaluable of type derivative_evaluable.
     */
    virtual levi::ExpressionComponent<derivative_evaluable> getColumnDerivative(Eigen::Index column, std::shared_ptr<levi::VariableBase> variable) {
        return levi::ExpressionComponent<derivative_evaluable>();
    }

    /**
     * @brief Check whether the evaluable depends on a specified variable
     * @param variable The variable of interest
     * @return True if dependent
     *
     * User should override this method to reduce callings to getColumnDerivative.
     */
    virtual bool isDependentFrom(std::shared_ptr<levi::VariableBase> variable) {
        return true;
    }

    Evaluable<Matrix>& operator=(const Evaluable& other) = delete;

    void operator=(Evaluable&& other) = delete;

    Evaluable<Matrix>& operator+(const Evaluable& other) const  = delete;

    Evaluable<Matrix>& operator-(const Evaluable& other) const = delete;

    Evaluable<Matrix>& operator*(const Evaluable& other) const = delete;

};

/**
 * @brief Evaluable class (for scalar type)
 *
 * The evaluable class defines a block which can be evaluated. It can be a matrix or a scalar (double, float,..).
 * User can define his own evaluable by publicly inheriting from this class and overload the evaluate() method.
 * It will be on the user to define which value the evaluable will take.
 * The method getColumnDerivative has to be overloaded in order to specify the derivative.
 *
 * Take a look at the ExpressionComponent documentation to place an evaluable into an expression.
 */
template <typename Scalar>
class levi::Evaluable<Scalar, typename std::enable_if<std::is_arithmetic<Scalar>::value>::type> {

    /**
     * @brief Name of the evaluable
     */
    std::string m_name;

public:

    /**
     * @brief The storage type used in the evaluable.
     *
     * This corresponds also to the value_type, row_type and col_type.
     */
    typedef Scalar matrix_type;

    typedef Scalar value_type;

    static const Eigen::Index rows_at_compile_time = 1;

    static const Eigen::Index cols_at_compile_time = 1;

    typedef Scalar row_type;

    typedef Scalar col_type;

    /**
     * @brief The Evaluable type used to store the column derivative.
     */
    typedef Evaluable<Eigen::Matrix<value_type, 1, Eigen::Dynamic>> derivative_evaluable;


protected:

    /**
     * @brief The buffer used to store the values of the evaluable.
     */
    Scalar m_evaluationBuffer;

public:

    Evaluable() = delete;

    /**
     * @brief Constructor
     * @param  Name of the evaluable
     *
     * The evaluation buffer (m_evaluationBuffer) is not initialized.
     */
    Evaluable(const std::string& name)
        : m_name(name)
    { }

    /**
     * @brief Constructor (for compatibility with the matrix version)
     * @param rows The number of rows (has to be equal to 1)
     * @param cols The number of cols (has to be equal to 1)
     * @param name Name of the evaluable.
     */
    Evaluable(Eigen::Index rows, Eigen::Index cols, const std::string& name)
        : m_name(name)
    {
        assert(rows == 1 && cols == 1);
        m_evaluationBuffer = 0;
    }

    /**
     * @brief Constructor
     * @param initialValue Initial value of the evaluation buffer.
     * @param name Name of the evaluable.
     */
    Evaluable(const Scalar& initialValue, const std::string& name)
        : m_name(name)
        , m_evaluationBuffer(initialValue)
    { }

    /**
     * @brief Constructor
     * @param initialValue Initial value of the evaluation buffer. The name will correspond to this value.
     */
    Evaluable(const Scalar& initialValue)
        : m_name(std::to_string(initialValue))
        , m_evaluationBuffer(initialValue)
    { }

    template <typename OtherMatrix, typename OtherDerivativeEvaluable>
    Evaluable(const Evaluable<OtherMatrix, OtherDerivativeEvaluable>& other) = delete;

    template <typename OtherMatrix, typename OtherDerivativeEvaluable>
    Evaluable(Evaluable<OtherMatrix, OtherDerivativeEvaluable>&& other) = delete;

    virtual ~Evaluable() { }

    /**
     * @brief Number of rows of the evaluable
     * @return the number of rows of the evaluable
     */
    Eigen::Index rows() const {
        return 1;
    }

    /**
     * @brief Number of cols of the evaluable
     * @return the number of cols of the evaluable
     */
    Eigen::Index cols() const {
        return 1;
    }

    /**
     * @brief Return the name of the evaluable
     * @return The name of the evaluable
     */
    std::string name() const {
        return m_name;
    }

    /**
     * @brief Evaluate the evaluable
     *
     * User should override this method to define custom evaluable.
     *
     * @return const reference to the evaluation buffer.
     */
    virtual const Scalar& evaluate() = 0;

    /**
     * @brief Get the derivative of a specified column with respect to a specified variable
     * @param column The index with respect to the derivative has to be computed.
     * @param variable The variable with respect to the derivative has to be computed.
     * @return An expression containing an evaluable of type derivative_evaluable.
     */
    virtual levi::ExpressionComponent<derivative_evaluable> getColumnDerivative(Eigen::Index column, std::shared_ptr<levi::VariableBase> variable) {
        return levi::ExpressionComponent<derivative_evaluable>();
    }

    /**
     * @brief Check whether the evaluable depends on a specified variable
     * @param variable The variable of interest
     * @return True if dependent
     *
     * User should override this method to reduce callings to getColumnDerivative.
     */
    virtual bool isDependentFrom(std::shared_ptr<levi::VariableBase> variable) {
        return true;
    }

    Evaluable<Scalar>& operator=(const Evaluable& other) = delete;

    void operator=(Evaluable&& other) = delete;

    Evaluable<Scalar>& operator+(const Evaluable& other) const  = delete;

    Evaluable<Scalar>& operator-(const Evaluable& other) const = delete;

    Evaluable<Scalar>& operator*(const Evaluable& other) const = delete;

};

#endif // LEVI_EVALUABLE_H