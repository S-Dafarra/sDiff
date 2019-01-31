/*
 * Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia
 * Authors: Stefano Dafarra
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 *
 */
#ifndef LEVI_ADVANCEDCONSTRUCTORS_H
#define LEVI_ADVANCEDCONSTRUCTORS_H

#include <levi/ForwardDeclarations.h>
#include <levi/Evaluable.h>
#include <levi/VariableBase.h>
#include <levi/Expression.h>

/**
 * The ConstructorByRows.
 *
 * Constructs an evaluable by stacking the specified rows in the constructor.
 */
template <typename EvaluableT>
class levi::ConstructorByRows : public levi::Evaluable<typename EvaluableT::matrix_type> {

    std::vector<levi::ExpressionComponent<levi::Evaluable<typename EvaluableT::row_type>>> m_rows;
    std::vector<levi::ExpressionComponent<typename levi::Evaluable<typename EvaluableT::row_type>::derivative_evaluable>> m_derivatives;

public:

    ConstructorByRows(const std::vector<levi::ExpressionComponent<levi::Evaluable<typename EvaluableT::row_type>>>& rows, std::string name)
        : levi::Evaluable<typename EvaluableT::matrix_type>(name)
        , m_rows(rows)
    {
        assert(m_rows.size() != 0);
        assert((EvaluableT::rows_at_compile_time == Eigen::Dynamic) || (EvaluableT::rows_at_compile_time == m_rows.size()));
        Eigen::Index nCols;

        nCols = m_rows.front().cols();

        for (size_t i = 1; i < m_rows.size(); ++i) {
            assert(m_rows[i].cols() == nCols);
        }

        this->resize(m_rows.size(), nCols);

        m_derivatives.resize(m_rows.size());
    }

    virtual const typename EvaluableT::matrix_type& evaluate() final {
        for (size_t i = 0; i < m_rows.size(); ++i) {
            this->m_evaluationBuffer.row(i) = m_rows[i].evaluate();
        }
        return this->m_evaluationBuffer;
    }

    virtual levi::ExpressionComponent<typename EvaluableT::derivative_evaluable> getColumnDerivative(Eigen::Index column, std::shared_ptr<levi::VariableBase> variable) {

        for (size_t i = 0; i < m_rows.size(); ++i) {
            m_derivatives[i] = m_rows[i](0, column).getColumnDerivative(0, variable); //the i-th row of the column derivative corresponds to the (only) column derivative of the element (i, column)
        }

        levi::ExpressionComponent<typename EvaluableT::derivative_evaluable> derivative;

        derivative = levi::ExpressionComponent<typename EvaluableT::derivative_evaluable>::ComposeByRows(m_derivatives, "d(" + this->name() + ")/d" + variable->variableName());

        return derivative;
    }

    virtual bool isDependentFrom(std::shared_ptr<levi::VariableBase> variable) {
        bool isDependent = false;

        for (size_t i = 0; i < m_rows.size(); ++i) {
            isDependent = isDependent || m_rows[i].isDependentFrom(variable);
        }

        return isDependent;
    }

};

/**
 * The ConstructorByCols.
 *
 * Constructs an evaluable by aligning the columns specified in the constructor.
 */
template <typename EvaluableT>
class levi::ConstructorByCols : public levi::Evaluable<typename EvaluableT::matrix_type> {

    std::vector<levi::ExpressionComponent<levi::Evaluable<typename EvaluableT::col_type>>> m_cols;

public:

    ConstructorByCols(const std::vector<levi::ExpressionComponent<levi::Evaluable<typename EvaluableT::col_type>>>& cols, std::string name)
        : levi::Evaluable<typename EvaluableT::matrix_type>(name)
        , m_cols(cols)
    {
        assert(m_cols.size() != 0);
        assert((EvaluableT::cols_at_compile_time == Eigen::Dynamic) || (EvaluableT::cols_at_compile_time == m_cols.size()));
        Eigen::Index nRows;

        nRows = m_cols.front().rows();

        for (size_t i = 1; i < m_cols.size(); ++i) {
            assert(m_cols[i].rows() == nRows);
        }

        this->resize(nRows, m_cols.size());
    }

    virtual const typename EvaluableT::matrix_type& evaluate() final {
        for (size_t i = 0; i < m_cols.size(); ++i) {
            this->m_evaluationBuffer.col(i) = m_cols[i].evaluate();
        }
        return this->m_evaluationBuffer;
    }

    virtual levi::ExpressionComponent<typename EvaluableT::derivative_evaluable> getColumnDerivative(Eigen::Index column, std::shared_ptr<levi::VariableBase> variable) {

        levi::ExpressionComponent<typename EvaluableT::derivative_evaluable> derivative;

        derivative = m_cols[column].getColumnDerivative(0, variable);

        return derivative;
    }

    virtual bool isDependentFrom(std::shared_ptr<levi::VariableBase> variable) {
        bool isDependent = false;

        for (size_t i = 0; i < m_cols.size(); ++i) {
            isDependent = isDependent || m_cols[i].isDependentFrom(variable);
        }

        return isDependent;
    }

};

#endif // LEVI_ADVANCEDCONSTRUCTORS_H