/*
 * Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia
 * Authors: Stefano Dafarra
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 *
 */
#ifndef SDIFF_EXPRESSIONIMPLEMENTATION_H
#define SDIFF_EXPRESSIONIMPLEMENTATION_H

#include <sDiff/Expression.h>

template <class EvaluableT>
template<bool value>
void sDiff::ExpressionComponent<EvaluableT>::default_constructor(bool_value<value>)
{ }

template <class EvaluableT>
void sDiff::ExpressionComponent<EvaluableT>::default_constructor(bool_value<true>) {
    m_evaluable = std::make_shared<EvaluableT>();
}

template <class EvaluableT>
void sDiff::ExpressionComponent<EvaluableT>::default_constructor(bool_value<false>) {
    m_evaluable = nullptr;
}

template <class EvaluableT>
template<bool value, typename OtherEvaluable>
void sDiff::ExpressionComponent<EvaluableT>::casted_assignement(bool_value<value>, std::shared_ptr<OtherEvaluable> other) {}

template <class EvaluableT>
template<typename OtherEvaluable>
void sDiff::ExpressionComponent<EvaluableT>::casted_assignement(bool_value<true>, std::shared_ptr<OtherEvaluable> other) {
    m_evaluable = other;
}

template <class EvaluableT>
template<typename OtherEvaluable>
void sDiff::ExpressionComponent<EvaluableT>::casted_assignement(bool_value<false>, std::shared_ptr<OtherEvaluable> other) {
    m_evaluable = std::make_shared<sDiff::CastEvaluable<EvaluableT, OtherEvaluable>>(other);
}

template <class EvaluableT>
sDiff::ExpressionComponent<EvaluableT>::ExpressionComponent()
{
    default_constructor(bool_value<std::is_constructible<EvaluableT>::value>());
}

template <class EvaluableT>
template<class EvaluableOther>
sDiff::ExpressionComponent<EvaluableT>::ExpressionComponent(const ExpressionComponent<EvaluableOther>& other) {
    this = other;
}

template <class EvaluableT>
template<class EvaluableOther>
sDiff::ExpressionComponent<EvaluableT>::ExpressionComponent(ExpressionComponent<EvaluableOther>&& other) {
    *this = other;
}

template <class EvaluableT>
template<class... Args >
sDiff::ExpressionComponent<EvaluableT>::ExpressionComponent(Args&&... args)
    : m_evaluable(std::make_shared<EvaluableT>(args...))
{ }

template <class EvaluableT>
std::weak_ptr<EvaluableT> sDiff::ExpressionComponent<EvaluableT>::evaluable() {
    return m_evaluable;
}

template <class EvaluableT>
std::string sDiff::ExpressionComponent<EvaluableT>::name() const {
    assert(m_evaluable);
    return m_evaluable->name();
}

template <class EvaluableT>
Eigen::Index sDiff::ExpressionComponent<EvaluableT>::rows() const {
    assert(m_evaluable);
    return m_evaluable->rows();
}

template <class EvaluableT>
Eigen::Index sDiff::ExpressionComponent<EvaluableT>::cols() const {
    assert(m_evaluable);
    return m_evaluable->cols();
}

template <class EvaluableT>
const Eigen::MatrixBase<typename EvaluableT::matrix_type>& sDiff::ExpressionComponent<EvaluableT>::evaluate() {
    assert(m_evaluable);
    return m_evaluable->evaluate();
}

template <class EvaluableT>
template<class EvaluableRhs>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_sum_return<typename EvaluableT::matrix_type, typename EvaluableRhs::matrix_type>::type>> sDiff::ExpressionComponent<EvaluableT>::operator+(const ExpressionComponent<EvaluableRhs>& rhs) {
    assert(rows() == rhs.rows());
    assert(cols() == rhs.cols());
    assert(m_evaluable);
    assert(rhs.m_evaluable);

    ExpressionComponent<sDiff::Evaluable<
            typename matrix_sum_return<typename EvaluableT::matrix_type, typename EvaluableRhs::matrix_type>::type>> newExpression;

    newExpression = ExpressionComponent<SumEvaluable<EvaluableT, EvaluableRhs>>(this->m_evaluable, rhs.m_evaluable);

    return newExpression;
}

template <class EvaluableT>
template <typename Matrix>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_sum_return<typename EvaluableT::matrix_type, Matrix>::type>> sDiff::ExpressionComponent<EvaluableT>::operator+(const Matrix& rhs) {
    ExpressionComponent<ConstantEvaluable<Matrix>> constant = build_constant(bool_value<std::is_arithmetic<Matrix>::value>(), rhs);

    return operator+(constant);
}

template <class EvaluableT>
template<class EvaluableRhs>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_sum_return<typename EvaluableT::matrix_type, typename EvaluableRhs::matrix_type>::type>> sDiff::ExpressionComponent<EvaluableT>::operator-(const ExpressionComponent<EvaluableRhs>& rhs) {
    assert(rows() == rhs.rows());
    assert(cols() == rhs.cols());
    assert(m_evaluable);
    assert(rhs.m_evaluable);

    ExpressionComponent<sDiff::Evaluable<
            typename matrix_sum_return<typename EvaluableT::matrix_type, typename EvaluableRhs::matrix_type>::type>> newExpression;

    newExpression = ExpressionComponent<SubtractionEvaluable<EvaluableT, EvaluableRhs>>(this->m_evaluable, rhs.m_evaluable);

    return newExpression;
}

template <class EvaluableT>
template <typename Matrix>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_sum_return<typename EvaluableT::matrix_type, Matrix>::type>> sDiff::ExpressionComponent<EvaluableT>::operator-(const Matrix& rhs) {
    ExpressionComponent<ConstantEvaluable<Matrix>> constant = build_constant(bool_value<std::is_arithmetic<Matrix>::value>(), rhs);

    return operator-(constant);
}

template <class EvaluableT>
template<class EvaluableRhs>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_product_return<typename EvaluableT::matrix_type, typename EvaluableRhs::matrix_type>::type>> sDiff::ExpressionComponent<EvaluableT>::operator*(const ExpressionComponent<EvaluableRhs>& rhs) {
    assert((cols() == 1 && rows() == 1) || (rhs.cols() == 1 && rhs.rows() == 1) || (cols() == rhs.rows()) && "Dimension mismatch for product.");
    assert(m_evaluable);
    assert(rhs.m_evaluable);

    ExpressionComponent<sDiff::Evaluable<typename matrix_product_return<typename EvaluableT::matrix_type, typename EvaluableRhs::matrix_type>::type>> newExpression;

    newExpression = ExpressionComponent<ProductEvaluable<EvaluableT, EvaluableRhs>>(this->m_evaluable, rhs.m_evaluable);

    return newExpression;
}

template <class EvaluableT>
template <typename Matrix>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_product_return<typename EvaluableT::matrix_type, Matrix>::type>> sDiff::ExpressionComponent<EvaluableT>::operator*(const Matrix& rhs) {
    ExpressionComponent<ConstantEvaluable<Matrix>> constant = build_constant(bool_value<std::is_arithmetic<Matrix>::value>(), rhs);

    return operator*(constant);
}

template <class EvaluableT>
template<class EvaluableRhs>
sDiff::ExpressionComponent<EvaluableT>& sDiff::ExpressionComponent<EvaluableT>::operator=(const ExpressionComponent<EvaluableRhs>& rhs) {
    static_assert (!std::is_base_of<sDiff::EvaluableVariable<typename EvaluableT::matrix_type>, EvaluableT>::value, "Cannot assign an expression to a variable." );
    casted_assignement(bool_value<std::is_base_of<EvaluableT, EvaluableRhs>::value>(), rhs.m_evaluable);
    return *this;
}

template <class EvaluableT>
template<class EvaluableRhs>
sDiff::ExpressionComponent<EvaluableT>& sDiff::ExpressionComponent<EvaluableT>::operator=(const ExpressionComponent<EvaluableRhs>&& rhs) {
    static_assert (!std::is_base_of<sDiff::EvaluableVariable<typename EvaluableT::matrix_type>, EvaluableT>::value, "Cannot assign an expression to a variable." );
    casted_assignement(bool_value<std::is_base_of<EvaluableT, EvaluableRhs>::value>(), rhs.m_evaluable);
    return *this;
}

//assign from a constant
template <class EvaluableT>
template<typename Matrix>
void sDiff::ExpressionComponent<EvaluableT>::operator=(const Matrix& rhs) {
    static_assert (has_equal_to_constant_operator<Matrix>(), "This expression cannot be set equal to a constant.");
    assert(m_evaluable && "This expression cannot be set because the constructor was not called properly.");
    (*m_evaluable) = rhs;
}

template <class EvaluableT>
sDiff::ExpressionComponent<sDiff::Evaluable<typename sDiff::RowEvaluable<EvaluableT>::row_type>> sDiff::ExpressionComponent<EvaluableT>::row(Eigen::Index row) {
    assert(row < this->rows());
    assert(m_evaluable && "Cannot extract a row from this expression");

    ExpressionComponent<RowEvaluable<EvaluableT>> selectedRow(m_evaluable, row);
    assert(selectedRow.m_evaluable);

    return selectedRow;
}

//end of ExpressionComponent implementation

template <typename Matrix, class EvaluableT>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_sum_return<typename EvaluableT::matrix_type, Matrix>::type>> operator+(const Matrix& lhs, const sDiff::ExpressionComponent<EvaluableT> &rhs) {
    sDiff::ExpressionComponent<sDiff::ConstantEvaluable<Matrix>> constant =
            sDiff::build_constant(bool_value<std::is_arithmetic<Matrix>::value>(), lhs);

    return constant + rhs;
}

template <typename Matrix, class EvaluableT>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_sum_return<typename EvaluableT::matrix_type, Matrix>::type>> operator-(const Matrix& lhs, const sDiff::ExpressionComponent<EvaluableT> &rhs) {
    sDiff::ExpressionComponent<sDiff::ConstantEvaluable<Matrix>> constant =
            sDiff::build_constant(bool_value<std::is_arithmetic<Matrix>::value>(), lhs);

    return constant - rhs;
}

template <typename Matrix, class EvaluableT>
sDiff::ExpressionComponent<sDiff::Evaluable<typename matrix_product_return<Matrix, typename EvaluableT::matrix_type>::type>> operator*(const Matrix& lhs, const sDiff::ExpressionComponent<EvaluableT> &rhs) {

    sDiff::ExpressionComponent<sDiff::ConstantEvaluable<Matrix>> newConstant =
            sDiff::build_constant<Matrix>(bool_value<std::is_arithmetic<Matrix>::value>(), lhs);

    return newConstant * rhs;
}

#endif // SDIFF_EXPRESSIONIMPLEMENTATION_H