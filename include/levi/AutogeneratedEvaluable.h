/*
* Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia
* Authors: Stefano Dafarra
* CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
*
*/
#ifndef LEVI_AUTOGENERATEDEVALUABLE_H
#define LEVI_AUTOGENERATEDEVALUABLE_H

#include <levi/AutogeneratedHelper.h>
#include <levi/CompiledEvaluable.h>

template<typename EvaluableT>
class levi::AutogeneratedEvaluable
    : public levi::Evaluable<Eigen::Matrix<typename EvaluableT::value_type, Eigen::Dynamic, Eigen::Dynamic>> {

public:

    using SqueezedMatrix = typename levi::TreeComponent<EvaluableT>::SqueezedMatrix;

private:

    using SqueezedMatrixRef = Eigen::Ref<SqueezedMatrix>;
    using base_type = levi::CompiledEvaluable<SqueezedMatrixRef, SqueezedMatrixRef>;

    levi::ExpressionComponent<EvaluableT> m_fullExpression;
    levi::CompiledEvaluableFactory<base_type> m_compiledEvaluable;

    levi::AutogeneratedHelper<EvaluableT> m_helper;

public:

    AutogeneratedEvaluable(const levi::ExpressionComponent<EvaluableT>& fullExpression, const std::string& name)
        : levi::Evaluable<SqueezedMatrix> (fullExpression.rows(), fullExpression.cols(), name)
          , m_fullExpression(fullExpression)
          , m_helper({fullExpression}, name)
    {
        std::string cleanName = m_helper.name();

        std::ostringstream header;
        header << "//This file has been autogenerated" << std::endl;
        header << "#ifndef LEVI_COMPILED"<< cleanName << "_H" << std::endl;
        header << "#define LEVI_COMPILED"<< cleanName << "_H" << std::endl;

        header << "#include<levi/CompiledEvaluable.h>" << std::endl << std::endl;
        header << "class " << cleanName << ": public " << type_name<base_type>() << " {" <<std::endl;
        header << "public:" << std::endl;
        header << "    virtual void evaluate(const std::vector<" << type_name<SqueezedMatrixRef>() << ">& generics, "
               << type_name<SqueezedMatrixRef>() << " output) final;" << std::endl;
        header << "};" << std::endl;
        header << "#endif //LEVI_COMPILED"<< cleanName << "_H" << std::endl;

        std::ostringstream cpp;
        cpp << "void " << cleanName << "::evaluate(const std::vector<" << type_name<SqueezedMatrixRef>() << ">& generics, "
            << type_name<SqueezedMatrixRef>() << " output) {" << std::endl;
        cpp << m_helper.getHelpersDeclaration().str() << std::endl;
        cpp << m_helper.getCommonsDeclaration().str() << std::endl;
        if (this->rows() == 1 && this->cols() == 1) {
            cpp << "    output(0, 0) = ";
        } else {
            cpp << "    output = ";
        }
        cpp << m_helper.getFinalExpressions()[0].str() << ";" << std::endl << "}" << std::endl;

        m_helper.compile(header.str(), cpp.str(), m_compiledEvaluable, cleanName);

    }

    ~AutogeneratedEvaluable();

    virtual const SqueezedMatrix& evaluate() final {

        m_compiledEvaluable->evaluate(m_helper.evaluateGenerics(), this->m_evaluationBuffer);

        return this->m_evaluationBuffer;
    }

    virtual bool isNew(size_t callerID) final {

        bool newVal = m_helper.checkGenerics();

        if (newVal) {
            this->resetEvaluationRegister();
        }

        return !this->m_evaluationRegister[callerID];
    }

};

template<typename EvaluableT>
levi::AutogeneratedEvaluable<EvaluableT>::~AutogeneratedEvaluable() { }


#endif // LEVI_AUTOGENERATEDEVALUABLE_H
