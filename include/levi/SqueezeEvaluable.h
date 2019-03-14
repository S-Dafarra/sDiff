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
#include <levi/autogenerated/Path.h>

#include <cppfs/fs.h>
#include <cppfs/FileHandle.h>
#include <cppfs/FilePath.h>

#include <cstddef>
#include <stdexcept>
#include <cstring>
#include <ostream>
#include <cstdlib>

//Taken from https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c
class static_string
{
    const char* const p_;
    const std::size_t sz_;

public:
    typedef const char* const_iterator;

    template <std::size_t N>
    constexpr static_string(const char(&a)[N]) noexcept
        : p_(a)
        , sz_(N-1)
    {}

    constexpr static_string(const char* p, std::size_t N) noexcept
        : p_(p)
        , sz_(N)
    {}

    constexpr const char* data() const noexcept {return p_;}
    constexpr std::size_t size() const noexcept {return sz_;}

    constexpr const_iterator begin() const noexcept {return p_;}
    constexpr const_iterator end()   const noexcept {return p_ + sz_;}

    constexpr char operator[](std::size_t n) const
    {
        return n < sz_ ? p_[n] : throw std::out_of_range("static_string");
    }
};

inline std::ostream& operator<<(std::ostream& os, static_string const& s)
{
    return os.write(s.data(), s.size());
}

template <class T>
constexpr static_string type_name()
{
#ifdef __clang__
    static_string p = __PRETTY_FUNCTION__;
    return static_string(p.data() + 31, p.size() - 31 - 1);
#elif defined(__GNUC__)
    static_string p = __PRETTY_FUNCTION__;
#  if __cplusplus < 201402
    return static_string(p.data() + 36, p.size() - 36 - 1);
#  else
    return static_string(p.data() + 46, p.size() - 46 - 1);
#  endif
#elif defined(_MSC_VER)
    static_string p = __FUNCSIG__;
    return static_string(p.data() + 38, p.size() - 38 - 7);
#endif
}

template<typename EvaluableT>
class levi::SqueezeEvaluable
    : public levi::Evaluable<Eigen::Matrix<typename EvaluableT::value_type, Eigen::Dynamic, Eigen::Dynamic>> {

public:

    typedef Eigen::Matrix<typename EvaluableT::value_type, Eigen::Dynamic, Eigen::Dynamic> SqueezedMatrix;

private:

    using Type = levi::EvaluableType;

    class SqueezedComponent {
    public:
        levi::ExpressionComponent<levi::Evaluable<SqueezedMatrix>> partialExpression;

        Type type;

        SqueezedMatrix buffer;

        size_t lhsIndex;
        size_t rhsIndex;

        levi::BlockType block;
        typename EvaluableT::value_type exponent;


        SqueezedComponent(const levi::ExpressionComponent<levi::Evaluable<SqueezedMatrix>>& expression)
            : partialExpression(expression)
              , type(expression.info().type)
              , lhsIndex(0)
              , rhsIndex(0)
        {
            buffer.resize(expression.rows(), expression.cols());

            if (type != Type::Generic) {
                block = expression.info().block;
                exponent = expression.info().exponent;
            }

            if (type == Type::Null) {
                buffer.setZero();
            }

            if (type == Type::Identity) {
                buffer.setIdentity();
            }
        }

    };

    struct LiteralComponent {
        std::string literal;
        bool isScalar;
    };

    std::vector<SqueezedComponent> m_expandedExpression;
    std::vector<LiteralComponent> m_literalSubExpressions;

    std::vector<size_t> m_generics;

    levi::ExpressionComponent<EvaluableT> m_fullExpression;

    size_t expandTree(const levi::ExpressionComponent<levi::Evaluable<SqueezedMatrix>>& node) {

        //Returns the position in which the node has been saved
        assert(node.isValidExpression());

        levi::EvaluableType type;

        type = node.info().type;

        m_expandedExpression.emplace_back(node);

        size_t currentIndex = currentIndex = m_expandedExpression.size() - 1;

        if (type == Type::Generic) {
            for (size_t generic : m_generics) {
                if (m_expandedExpression[generic].partialExpression == m_expandedExpression[currentIndex].partialExpression) {
                    // This is the case where the same Generic has been added before. The problem is that the casting makes the same evaluable look different.
                    m_expandedExpression.pop_back();
                    return generic;
                }
            }
            m_generics.push_back(currentIndex);
            return currentIndex;
        }

        if (type == Type::Null || type == Type::Identity) {
            return currentIndex;
        }

        if (type == Type::Sum || type == Type::Subtraction || type == Type::Product || type == Type::Division) {
            size_t lhsIndex = expandTree(node.info().lhs);
            m_expandedExpression[currentIndex].lhsIndex = lhsIndex;
            size_t rhsIndex = expandTree(node.info().rhs);
            m_expandedExpression[currentIndex].rhsIndex = rhsIndex;
            return currentIndex;
        }

        if (type == Type::InvertedSign || type == Type::Pow || type == Type::Transpose || type == Type::Row ||
            type == Type::Column || type == Type::Element || type == Type::Block) {
            size_t lhsIndex = expandTree(node.info().lhs);
            m_expandedExpression[currentIndex].lhsIndex = lhsIndex;
            return currentIndex;
        }

        assert(false && "Case not considered.");
        return m_expandedExpression.size();
    }

    void getLiteralExpression() {
        Type type;
        m_literalSubExpressions.resize(m_expandedExpression.size());

        for (size_t generic = 0; generic < m_generics.size(); ++generic) {
            if (m_expandedExpression[m_generics[generic]].buffer.rows() == 1 && m_expandedExpression[m_generics[generic]].buffer.cols() == 1) {
                m_literalSubExpressions[m_generics[generic]].literal = "generics[" + std::to_string(generic) + "](0,0)";
                m_literalSubExpressions[m_generics[generic]].isScalar = true;
            } else {
                m_literalSubExpressions[m_generics[generic]].literal = "generics[" + std::to_string(generic) + "]";
                m_literalSubExpressions[m_generics[generic]].isScalar = false;
            }
        }

        for(int i = m_expandedExpression.size() - 1; i >= 0; --i) {

            SqueezedComponent& subExpr = m_expandedExpression[static_cast<size_t>(i)];
            LiteralComponent& literalSubExpr = m_literalSubExpressions[static_cast<size_t>(i)];
            const LiteralComponent& lhs = m_literalSubExpressions[subExpr.lhsIndex];
            const LiteralComponent& rhs = m_literalSubExpressions[subExpr.rhsIndex];

            type = subExpr.type;

            if (type == Type::Sum) {

                literalSubExpr.literal = "(" + lhs.literal + " + " + rhs.literal + ")";
                literalSubExpr.isScalar = (lhs.isScalar && rhs.isScalar);

            } else if (type == Type::Subtraction) {

                literalSubExpr.literal = "(" + lhs.literal + " - " + rhs.literal + ")";
                literalSubExpr.isScalar = (lhs.isScalar && rhs.isScalar);

            } else if (type == Type::Product) {

                literalSubExpr.literal = lhs.literal + " * " + rhs.literal;
                literalSubExpr.isScalar = (lhs.isScalar && rhs.isScalar);

            } else if (type == Type::Division) {

                literalSubExpr.literal = lhs.literal + " / " + rhs.literal;
                literalSubExpr.isScalar = lhs.isScalar;

            } else if (type == Type::InvertedSign) {

                literalSubExpr.literal = "-" + lhs.literal;
                literalSubExpr.isScalar = lhs.isScalar;

            } else if (type == Type::Pow) {

                literalSubExpr.literal = "std::pow(" + lhs.literal +", " + std::to_string(subExpr.exponent) + ")";
                literalSubExpr.isScalar = true;

            } else if (type == Type::Transpose) {

                assert(!lhs.isScalar);
                literalSubExpr.literal = "(" + lhs.literal + ").transpose()";
                literalSubExpr.isScalar = false;

            } else if (type == Type::Row) {

                assert(!lhs.isScalar);
                literalSubExpr.literal = "(" + lhs.literal + ").row(" + std::to_string(subExpr.block.startRow) + ")";
                literalSubExpr.isScalar = false;

            } else if (type == Type::Column) {

                assert(!lhs.isScalar);
                literalSubExpr.literal = "(" + lhs.literal + ").col(" + std::to_string(subExpr.block.startCol) + ")";
                literalSubExpr.isScalar = false;

            } else if (type == Type::Element) {

                assert(!lhs.isScalar);
                literalSubExpr.literal = "(" + lhs.literal + ")(" + std::to_string(subExpr.block.startRow) + ", " + std::to_string(subExpr.block.startCol) + ")";
                literalSubExpr.isScalar = true;

            } else if (type == Type::Block) {

                assert(!lhs.isScalar);
                literalSubExpr.literal = "(" + lhs.literal + ").block<" + std::to_string(subExpr.block.rows) + ", " + std::to_string(subExpr.block.cols) + ">(" + std::to_string(subExpr.block.startRow) + ", " + std::to_string(subExpr.block.startCol) + ")";
                literalSubExpr.isScalar = false;

            }

            if (!literalSubExpr.isScalar && (subExpr.buffer.rows() == 1) && (subExpr.buffer.cols() == 1)) { //the subexpression is an Eigen object but of dimension 1x1
                literalSubExpr.literal = "(" + literalSubExpr.literal + ")(0,0)";
                literalSubExpr.isScalar = true;
            }
        }
    }

public:

    SqueezeEvaluable(const levi::ExpressionComponent<EvaluableT>& fullExpression, const std::string& name)
        : levi::Evaluable<SqueezedMatrix> (fullExpression.rows(), fullExpression.cols(), name)
          , m_fullExpression(fullExpression)
    {
        m_expandedExpression.clear();
        m_generics.clear();
        expandTree(fullExpression);

        getLiteralExpression();

        std::string cleanName = name;

        for (char& letter : cleanName) {
            if (letter == ' ') {
                letter = '_';
            } else if (!(((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) || ((letter >= '0') && (letter <= '9')))) {
                letter = '-';
            }
        }

        cppfs::FileHandle dir  = cppfs::fs::open("./" + cleanName);
        std::string leviListDir = LEVI_AUTOGENERATED_DIR;
        cppfs::FileHandle list = cppfs::fs::open(leviListDir + "/CMakeLists.auto");
        assert(list.isFile());

        if (!dir.isDirectory()) {
            bool dirCreated = dir.createDirectory();
            assert(dirCreated && "Unable to create folder.");
        }

        cppfs::FileHandle copiedList = dir.open("CMakeLists.txt");
        list.copy(copiedList);

        cppfs::FileHandle header = dir.open("source.h");
        auto headerStream = header.createOutputStream(std::ios_base::trunc);

        *headerStream << "//This file has been autogenerated" << std::endl;
        *headerStream << "#ifndef LEVI_COMPILED"<< cleanName << "_H" << std::endl;
        *headerStream << "#define LEVI_COMPILED"<< cleanName << "_H" << std::endl;

        *headerStream << "#include<levi/CompiledEvaluable.h>" << std::endl << std::endl;
        *headerStream << "class " << cleanName << ": public levi::CompiledEvaluable<" << type_name<SqueezedMatrix>()
                      << ", " << type_name<typename EvaluableT::matrix_type>() << "> {" <<std::endl;
        *headerStream << "public:" << std::endl;
        *headerStream << "    virtual void evaluate(const std::vector<" << type_name<SqueezedMatrix>() << ">& generics, "
                      << type_name<typename EvaluableT::matrix_type>() << "& output) override;" << std::endl;
        *headerStream << "};" << std::endl;
        *headerStream << "#endif //LEVI_COMPILED"<< cleanName << "_H" << std::endl;


        cppfs::FileHandle cpp = dir.open("source.cpp");
        auto cppStream = cpp.createOutputStream(std::ios_base::trunc);

        *cppStream << "//This file has been autogenerated" << std::endl;
        *cppStream << "#include \"source.h\" " << std::endl << std::endl;
        *cppStream << "void " << cleanName << "::evaluate(const std::vector<" << type_name<SqueezedMatrix>() << ">& generics, "
                << type_name<typename EvaluableT::matrix_type>() << "& output) {" << std::endl;
        *cppStream << "    output = ";
        *cppStream << m_literalSubExpressions[0].literal << ";" << std::endl << "}" << std::endl;

        cppfs::FileHandle buildDir = cppfs::fs::open("./" + cleanName + "/build");

        if (!buildDir.isDirectory()) {
            bool dirCreated = buildDir.createDirectory();
            assert(dirCreated && "Unable to create build folder.");
        }

        std::string buildCommand = "cmake -B" + buildDir.path() + " -H" + dir.path();

        int ret = std::system(buildCommand.c_str());

        if(WEXITSTATUS(ret) != EXIT_SUCCESS) {
            assert(false && "The cmake configuration failed");
        }

        buildCommand = "cmake --build " + buildDir.path() + " --config Release";
        ret = std::system(buildCommand.c_str());

        if(WEXITSTATUS(ret) != EXIT_SUCCESS) {
            assert(false && "The compilation failed");
        }
    }

    ~SqueezeEvaluable();

    virtual const SqueezedMatrix& evaluate() final {

        for (size_t generic : m_generics) {
            m_expandedExpression[generic].buffer = m_expandedExpression[generic].partialExpression.evaluate(false); //first evaluate generics
        }

        levi::EvaluableType type;

        for(typename std::vector<SqueezedComponent>::reverse_iterator i = m_expandedExpression.rbegin();
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
levi::SqueezeEvaluable<EvaluableT>::~SqueezeEvaluable() { }

#endif // LEVI_SQUEEZEEVALUABLE_H
