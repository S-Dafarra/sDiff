/*
* Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia
* Authors: Stefano Dafarra
* CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
*
*/
#ifndef LEVI_AUTOGENERATEDEVALUABLE_H
#define LEVI_AUTOGENERATEDEVALUABLE_H


#include <levi/HelpersForwardDeclarations.h>
#include <levi/ForwardDeclarations.h>
#include <levi/Expression.h>
#include <levi/TreeExpander.h>
#include <levi/TypeDetector.h>
#include <levi/CompiledEvaluable.h>
#include <levi/autogenerated/Path.h>

#include <levi/external/zupply.h>
#include <shlibpp/SharedLibraryClass.h>
#include <shlibpp/SharedLibrary.h>

#include <cstddef>
#include <stdexcept>
#include <cstring>
#include <ostream>
#include <cstdlib>

#include <fstream>

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
class levi::AutogeneratedEvaluable
    : public levi::Evaluable<Eigen::Matrix<typename EvaluableT::value_type, Eigen::Dynamic, Eigen::Dynamic>> {

public:

    using SqueezedMatrix = typename levi::TreeExpander<EvaluableT>::SqueezedMatrix;

private:

    using Type = levi::EvaluableType;

    struct LiteralComponent {
        std::string literal;
        bool isScalar;
    };

    std::vector<LiteralComponent> m_literalSubExpressions;
    levi::TreeExpander<EvaluableT> m_tree;
    levi::ExpressionComponent<EvaluableT> m_fullExpression;
    std::map<std::string, std::string> m_helpersVariables;
    std::ostringstream m_helpersDeclarations;

    using SqueezedMatrixRef = Eigen::Ref<SqueezedMatrix>;
    using base_type = levi::CompiledEvaluable<SqueezedMatrixRef, SqueezedMatrixRef>;
    base_type* m_compiledEvaluable;
    shlibpp::SharedLibraryClassFactory<base_type> m_compiledEvaluableFactory;
    std::vector<SqueezedMatrixRef> m_genericsRefs;

    std::string getScalarVariable(const std::string& originalExpression) {
        std::map<std::string, std::string>::iterator element = m_helpersVariables.find(originalExpression);

        if (element != m_helpersVariables.end()) {
            return (element->second);
        } else {
            std::pair<std::string, std::string> newElement;
            newElement.first = originalExpression;
            newElement.second = "m_helper" + std::to_string(m_helpersVariables.size());
            m_helpersDeclarations << "    " << type_name<typename EvaluableT::value_type>() << " " << newElement.second
                                  << " = " << newElement.first << ";" << std::endl;
            auto result = m_helpersVariables.insert(newElement);
            assert(result.second);
            return (result.first->second);
        }
    }

    void getLiteralExpression() {
        Type type;
        m_literalSubExpressions.resize(m_tree.expandedExpression.size());
        m_helpersVariables.clear();

        for (size_t generic = 0; generic < m_tree.generics.size(); ++generic) {
            if (m_tree.expandedExpression[m_tree.generics[generic]].buffer.rows() == 1 && m_tree.expandedExpression[m_tree.generics[generic]].buffer.cols() == 1) {
                m_literalSubExpressions[m_tree.generics[generic]].literal = "generics[" + std::to_string(generic) + "](0,0)";
                m_literalSubExpressions[m_tree.generics[generic]].isScalar = true;
            } else {
                m_literalSubExpressions[m_tree.generics[generic]].literal = "generics[" + std::to_string(generic) + "]";
                m_literalSubExpressions[m_tree.generics[generic]].isScalar = false;
            }
        }

        for(int i = m_tree.expandedExpression.size() - 1; i >= 0; --i) {

            levi::TreeComponent<EvaluableT>& subExpr = m_tree.expandedExpression[static_cast<size_t>(i)];
            LiteralComponent& literalSubExpr = m_literalSubExpressions[static_cast<size_t>(i)];
            const LiteralComponent& lhs = m_literalSubExpressions[subExpr.lhsIndex];
            const LiteralComponent& rhs = m_literalSubExpressions[subExpr.rhsIndex];

            type = subExpr.type;

            if (type == Type::Sum) {

                if (lhs.literal.size() + rhs.literal.size() > 150) {
                    std::ostringstream ss;
                    ss << "(" + lhs.literal + " +" << std::endl << "        " << rhs.literal << ")";
                    literalSubExpr.literal = ss.str();
                } else {
                    literalSubExpr.literal = "(" + lhs.literal + " + " + rhs.literal + ")";
                }

                literalSubExpr.isScalar = (lhs.isScalar && rhs.isScalar);

            } else if (type == Type::Subtraction) {

                if (lhs.literal.size() + rhs.literal.size() > 150) {
                    std::ostringstream ss;
                    ss << "(" << lhs.literal << " -" << std::endl << "        " << rhs.literal << ")";
                    literalSubExpr.literal = ss.str();
                } else {
                    literalSubExpr.literal = "(" + lhs.literal + " - " + rhs.literal + ")";
                }
                literalSubExpr.isScalar = (lhs.isScalar && rhs.isScalar);

            } else if (type == Type::Product) {

                if (lhs.literal.size() + rhs.literal.size() > 150) {
                    std::ostringstream ss;
                    ss << "(" << lhs.literal << " *" << std::endl << "        " << rhs.literal << ")";
                    literalSubExpr.literal = ss.str();
                } else {
                    literalSubExpr.literal = lhs.literal + " * " + rhs.literal;
                }
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

            if (literalSubExpr.isScalar) {
                literalSubExpr.literal = getScalarVariable(literalSubExpr.literal);
            }
        }
    }

public:

    AutogeneratedEvaluable(const levi::ExpressionComponent<EvaluableT>& fullExpression, const std::string& name)
        : levi::Evaluable<SqueezedMatrix> (fullExpression.rows(), fullExpression.cols(), name)
          , m_fullExpression(fullExpression)
          , m_tree(fullExpression)

    {
        getLiteralExpression();

        std::string cleanName = name;

        for (char& letter : cleanName) {
            if (letter == ' ') {
                letter = '_';
            } else if (!(((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) || ((letter >= '0') && (letter <= '9')))) {
                letter = '-';
            }
        }

        std::string workingDir = zz::os::current_working_directory() + "/" + cleanName;

        zz::fs::Path dir(workingDir);
        if (!dir.exist()) {
            bool dirCreated = zz::os::create_directory(workingDir);
            assert(dirCreated);
        }

        std::string leviListDir = LEVI_AUTOGENERATED_DIR;

        std::string CMakeSource = leviListDir + "/CMakeLists.auto";
        std::string CMakeDest = workingDir + "/CMakeLists.txt";
        zz::os::copyfile(CMakeSource, CMakeDest);

        std::string headerName = workingDir + "/source.h";
        std::fstream header(headerName.c_str(), std::ios::out | std::ios::trunc);

        assert(header.is_open());

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
        header.close();


        std::string cppName = workingDir + "/source.cpp";

        std::fstream cpp(cppName.c_str(), std::ios::out | std::ios::trunc);
        assert(cpp.is_open());

        cpp << "//This file has been autogenerated" << std::endl;
        cpp << "#include <shlibpp/SharedLibraryClass.h>" << std::endl;
        cpp << "#include \"source.h\" " << std::endl;
        cpp << "typedef " << type_name<base_type>() << " base_type;" << std::endl;
        cpp << "SHLIBPP_DEFINE_SHARED_SUBCLASS(" << cleanName << "Factory, "  << cleanName <<", base_type);"  << std::endl << std::endl;
        cpp << "void " << cleanName << "::evaluate(const std::vector<" << type_name<SqueezedMatrixRef>() << ">& generics, "
            << type_name<SqueezedMatrixRef>() << " output) {" << std::endl;
        cpp << m_helpersDeclarations.str() << std::endl;
        if (this->rows() == 1 && this->cols() == 1) {
            cpp << "    output(0, 0) = ";
        } else {
            cpp << "    output = ";
        }
        cpp << m_literalSubExpressions[0].literal << ";" << std::endl << "}" << std::endl;
        cpp.close();

        std::string buildDir = workingDir + "/build";
        zz::fs::Path buildDirPath(buildDir);
        if (!buildDirPath.exist()) {
            bool dirCreated = zz::os::create_directory(buildDir);
            assert(dirCreated);
        } else {
#ifdef _MSC_VER
            zz::os::remove_dir(buildDir + "\\lib\\Release");
#else
            zz::os::remove_dir(buildDir + "/lib");
#endif
        }

        std::string buildCommand = "cmake -B" + buildDir + " -H" + workingDir;

        int ret = std::system(buildCommand.c_str());
        assert(ret== EXIT_SUCCESS && "The cmake configuration failed");

        buildCommand = "cmake --build " + buildDir + " --config Release";

        ret = std::system(buildCommand.c_str());
        assert(ret== EXIT_SUCCESS && "The compilation failed");

#ifdef _MSC_VER
        zz::fs::Path libDir(buildDir + "\\lib\\Release", true);
#else
        zz::fs::Path libDir(buildDir + "/lib");
#endif
        size_t attempts = 0;
        while (!(libDir.is_dir() && !libDir.empty()) && attempts < 1e6) {
            attempts++;
            assert(attempts != 1e6 && "The library file was not created.");

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(5us);
        }

        m_compiledEvaluableFactory.extendSearchPath(buildDir + "\\lib\\Release");
        m_compiledEvaluableFactory.extendSearchPath(buildDir + "/lib");


        bool isLibOpen = m_compiledEvaluableFactory.open((cleanName + "Lib").c_str(), (cleanName + "Factory").c_str());

        if (!isLibOpen) {
            printf("error (%d) : %s\n", static_cast<std::uint32_t>(m_compiledEvaluableFactory.getStatus()),
                   m_compiledEvaluableFactory.getError().c_str());
        }

        assert(isLibOpen && m_compiledEvaluableFactory.isValid() && "Unable to open compiled shared library.");

        m_compiledEvaluable = m_compiledEvaluableFactory.create();

        assert(m_compiledEvaluable != nullptr && "The compiled instance is not valid.");

        for (size_t generic : m_tree.generics) {
            m_genericsRefs.emplace_back(m_tree.expandedExpression[generic].buffer);
        }

    }

    ~AutogeneratedEvaluable();

    virtual const SqueezedMatrix& evaluate() final {

        for (size_t generic : m_tree.generics) {
            m_tree.expandedExpression[generic].buffer = m_tree.expandedExpression[generic].partialExpression.evaluate(false); //first evaluate generics
        }

        m_compiledEvaluable->evaluate(m_genericsRefs, this->m_evaluationBuffer);

        return this->m_evaluationBuffer;
    }

    virtual bool isNew(size_t callerID) final {

        bool newVal = false;
        bool isNew;

        for (size_t i = 0; i < m_tree.generics.size(); ++i) {
            isNew = m_tree.expandedExpression[m_tree.generics[i]].partialExpression.isNew();
            newVal = isNew || newVal;
        }

        if (newVal) {
            this->resetEvaluationRegister();
        }

        return !this->m_evaluationRegister[callerID];
    }

    };

    template<typename EvaluableT>
    levi::AutogeneratedEvaluable<EvaluableT>::~AutogeneratedEvaluable() {
        m_compiledEvaluableFactory.destroy(m_compiledEvaluable);
    }


#endif // LEVI_AUTOGENERATEDEVALUABLE_H
