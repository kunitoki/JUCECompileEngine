/*
  ==============================================================================

  - JUCECompileEngine - Copyright (c) 2016, Lucio Asnaghi
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its contributors
     may be used to endorse or promote products derived from this software without
     specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
  OF THE POSSIBILITY OF SUCH DAMAGE.

  ==============================================================================
*/

#pragma once

#include "Common.h"
#include "SharedQueue.h"

#undef DEBUG
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/TextDiagnostic.h"
#include "clang/Serialization/ASTReader.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/BitCode/ReaderWriter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

//==============================================================================
using namespace clang;
using namespace clang::driver;

using ModulePtr = std::unique_ptr<llvm::Module>;
using ModulePtrList = std::vector<ModulePtr>;

//==============================================================================
std::string getExecutablePath(const char* Argv0);

//==============================================================================
enum class CompilationStatus
{
    NotNeeded,
    Ok,
    Error
};

//==============================================================================
enum class MessageEvents
{
    NoEvent,
    ExitThread,
    CompileProject,
    UpdateActivities
};

//==============================================================================
struct LiveCodeChange
{
    int start;
    int end;
    String text;
};

//==============================================================================
class DiagnosticReporter;

//==============================================================================
class LiveCodeBuilderImpl : public Thread
{
public:
    LiveCodeBuilderImpl(SendMessageFunction sendFunction,
                        void* userInfo,
                        const String& projectID,
                        const String& cacheFolderPath);

    ~LiveCodeBuilderImpl();

    /** Public interface from shared library */
    void setBuildInfo(const ValueTree& data);

    void fileUpdated(const File& file, const String& optionalText = String());

    void fileChanged(const File& file, const Array<LiveCodeChange>& changes = Array<LiveCodeChange>());

    void fileReset(const File& file);

    void reloadComponents();

    void cleanAll();

    void launchApp();

    void foregroundProcess(bool parentActive);

    void pong();

    /** Thread implementation */
    void run() override;

    /** Post internal messages */
    void sendActivityListUpdate();
    void sendCompileProject();

    /** Post messages to Projucer */
    void sendMessage(const ValueTree& tree);

    /** Get the diagnostics object */
    DiagnosticReporter* getDiagnostics();

private:
    friend class CompileJob;
    friend class LinkJob;
    friend class CleanAllJob;
    friend class RunAppJob;

    CompilationStatus compileFileIfNeeded(const File& file,
                                          const String& string,
                                          bool useString,
                                          const Array<LiveCodeChange>& changes,
                                          bool useChanges,
                                          String& errorString);

    void buildProjectIfNeeded();
    void cleanAllFiles();

    void runApp();

    File getCacheSourceFile(const File& file) const;
    File getCacheBitCodeFile(const File& file) const;

    SendMessageFunction sendMessageFunction;
    void* callbackUserInfo;
    String juceProjectID;
    File juceCacheFolder;

    String systemPath;
    String userPath;
    StringArray defines;
    StringArray extraCompilerFlags;
    String extraDLLs;
    String juceModulesFolder;
    String utilsCppInclude;
    String clangIncludePath;

    Array<File> compileUnits;
    Array<File> userFiles;

    ThreadPool activitiesPool;
    SharedQueue<MessageEvents> messageQueue;

    // CLANG
    ModulePtr compileFile(const File& file);
    std::unique_ptr<CompilerInvocation> createCompilerInvocation(const File& file);
    std::unique_ptr<CodeGenAction> generateCode(const File& file);
    std::unique_ptr<llvm::ExecutionEngine> createExecutionEngine(ModulePtr module, std::string* errorString);

    llvm::llvm_shutdown_obj shutdownObject;
    IntrusiveRefCntPtr<DiagnosticOptions> diagOpts;
    DiagnosticReporter* diagClient;
    IntrusiveRefCntPtr<DiagnosticIDs> diagIdentifier;
    DiagnosticsEngine diagEngine;
    std::unique_ptr<Driver> compilerDriver;
    std::unique_ptr<CompilerInstance> compilerInstance;
    std::unique_ptr<llvm::LLVMContext> context;
    std::mutex modulesMutex;
    ModulePtrList modules;
};
