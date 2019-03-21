/*
* Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia
* Authors: Stefano Dafarra
* CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
*
*/
#ifndef LEVI_SQUEEZEEVALUABLE_H
#define LEVI_SQUEEZEEVALUABLE_H

#include <levi/HelpersForwardDeclarations.h>
#include <levi/ForwardDeclarations.h>
#include <levi/Expression.h>
#include <levi/TypeDetector.h>
#include <levi/TreeExpander.h>

template<typename EvaluableT>
class levi::SqueezeEvaluable
    : public levi::Evaluable<Eigen::Matrix<typename EvaluableT::value_type, Eigen::Dynamic, Eigen::Dynamic>> {

public:

    using SqueezedMatrix = typename levi::TreeComponent<EvaluableT>::SqueezedMatrix;

private:

    using Type = levi::EvaluableType;

    levi::ExpressionComponent<EvaluableT> m_fullExpression;
    std::vector<levi::TreeComponent<EvaluableT>> m_expandedExpression;
    std::vector<size_t> m_generics;

public:

    SqueezeEvaluable(const levi::ExpressionComponent<EvaluableT>& fullExpression, const std::string& name)
        : levi::Evaluable<SqueezedMatrix> (fullExpression.rows(), fullExpression.cols(), name)
          , m_fullExpression(fullExpression)
    {
        levi::expandTree(fullExpression, m_expandedExpression, m_generics);
    }

    ~SqueezeEvaluable();

    virtual const SqueezedMatrix& evaluate() final {

        for (size_t generic : m_generics) {
            m_expandedExpression[generic].buffer = m_expandedExpression[generic].partialExpression.evaluate(false); //first evaluate generics
        }

        levi::EvaluableType type;

        for(typename std::vector<levi::TreeComponent<EvaluableT>>::reverse_iterator i = m_expandedExpression.rbegin();
             i != m_expandedExpression.rend(); ++i) {
            type = i->type;

            if (type == Type::Sum) {
                i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer + m_expandedExpression[i->rhsIndex].buffer); //this is why I need to evaluate the expanded expression in reverse order
            } else if (type == Type::Subtraction) {
                i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer - m_expandedExpression[i->rhsIndex].buffer);
            } else if (type == Type::Product) {

                if (m_expandedExpression[i->lhsIndex].buffer.cols() != m_expandedExpression[i->rhsIndex].buffer.rows()) {
                    if (m_expandedExpression[i->lhsIndex].buffer.rows() == 1 && m_expandedExpression[i->lhsIndex].buffer.cols() == 1) {
                        i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer(0,0) * m_expandedExpression[i->rhsIndex].buffer);
                    } else {
                        i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer * m_expandedExpression[i->rhsIndex].buffer(0,0));
                    }
                } else {
                    i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer * m_expandedExpression[i->rhsIndex].buffer);
                }

            } else if (type == Type::Division) {
                i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer / m_expandedExpression[i->rhsIndex].buffer(0,0));
            } else if (type == Type::InvertedSign) {
                i->buffer.lazyAssign(-m_expandedExpression[i->lhsIndex].buffer);
            } else if (type == Type::Pow) {
                i->buffer(0,0) = std::pow(m_expandedExpression[i->lhsIndex].buffer(0,0), i->exponent);
            } else if (type == Type::Transpose) {
                i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer.transpose());
            } else if (type == Type::Row) {
                i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer.row(i->block.startRow));
            } else if (type == Type::Column) {
                i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer.col(i->block.startCol));
            } else if (type == Type::Element) {
                i->buffer(0,0) = m_expandedExpression[i->lhsIndex].buffer(i->block.startRow, i->block.startCol);
            } else if (type == Type::Block) {
                i->buffer.lazyAssign(m_expandedExpression[i->lhsIndex].buffer.block(i->block.startRow, i->block.startCol, i->block.rows, i->block.cols));
            }
        }

        this->m_evaluationBuffer = m_expandedExpression[0].buffer;

        return this->m_evaluationBuffer;
    }

    virtual bool isNew(size_t callerID) final {

        bool newVal = false;
        bool isNew;

        for (size_t i = 0; i < m_generics.size(); ++i) {
            isNew = m_expandedExpression[m_generics[i]].partialExpression.isNew();
            newVal = isNew || newVal;
        }

        if (newVal) {
            this->resetEvaluationRegister();
        }

        return !this->m_evaluationRegister[callerID];
    }

};

template<typename EvaluableT>
levi::SqueezeEvaluable<EvaluableT>::~SqueezeEvaluable() {}

#endif // LEVI_SQUEEZEEVALUABLE_H
