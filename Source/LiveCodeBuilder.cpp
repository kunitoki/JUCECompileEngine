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

#include "LiveCodeBuilder.h"

//==============================================================================
class DiagnosticReporter : public TextDiagnosticPrinter {
public:
    DiagnosticReporter(LiveCodeBuilderImpl& builder, raw_ostream &os, DiagnosticOptions *diags)
        : TextDiagnosticPrinter(os, diags)
        , livecodeBuilder(builder)
    {
    }

    void clear() override
    {
        TextDiagnosticPrinter::clear();

        errorMap.clear();
    }

    void BeginSourceFile(const LangOptions &LangOpts, const Preprocessor *PP = nullptr) override
    {
        TextDiagnosticPrinter::BeginSourceFile(LangOpts, PP);
    }

    void EndSourceFile() override
    {
        TextDiagnosticPrinter::EndSourceFile();
    }

    void finish() override
    {
        TextDiagnosticPrinter::finish();
    }

    bool IncludeInDiagnosticCounts() const override
    {
        return TextDiagnosticPrinter::IncludeInDiagnosticCounts();
    }

    void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, const Diagnostic &Info) override
    {
        TextDiagnosticPrinter::HandleDiagnostic(DiagLevel, Info);

        errorMap[DiagLevel].add(Info);
    }

    HashMap<DiagnosticsEngine::Level, Array<Diagnostic>>& getErrorMap()
    {
        return errorMap;
    }

protected:
    LiveCodeBuilderImpl& livecodeBuilder;
    HashMap<DiagnosticsEngine::Level, Array<Diagnostic>> errorMap;
};

//==============================================================================
class CompileJob : public ThreadPoolJob
{
public:
    CompileJob(LiveCodeBuilderImpl& liveCodeBuilder_,
               const File& file,
               const String& string,
               bool useString,
               const Array<LiveCodeChange>& changes,
               bool useChanges)
        : ThreadPoolJob(file.getFileName()),
          livecodeBuilder(liveCodeBuilder_),
          fileToCompile(file),
          stringToCompile(string),
          isUsingString(useString),
          changesToCompile(changes),
          isUsingChanges(useChanges)
    {
    }

    JobStatus runJob() override
    {
        String errorString;

        CompilationStatus status = livecodeBuilder.compileFileIfNeeded(fileToCompile,
                                                                       stringToCompile,
                                                                       isUsingString,
                                                                       changesToCompile,
                                                                       isUsingChanges,
                                                                       errorString);

        if (status == CompilationStatus::Error)
        {
            LOG(errorString);

            ValueTree l(MessageTypes::DIAGNOSTIC_LIST);

            auto diagnostics = livecodeBuilder.getDiagnostics();
            auto& errorMap = diagnostics->getErrorMap();
            for (auto& info : errorMap[DiagnosticsEngine::Error])
            {
                (void)info;
                //auto range = info.getRange(0);

                ValueTree v (MessageTypes::DIAGNOSTIC);
                v.setProperty (Ids::text, errorString, nullptr);
                v.setProperty (Ids::file, fileToCompile.getFullPathName(), nullptr);
                //v.setProperty (Ids::range, fileToCompile.getFullPathName() + ":" + String(beg) + ":" + String(end));
                //v.setProperty (Ids::type, (int) type);
                //if (associatedDiagnostic != nullptr)
                //    v.addChild (associatedDiagnostic->toValueTree(), 0);
                l.addChild(v, -1, nullptr);
            }

            livecodeBuilder.sendMessage(ValueTree(MessageTypes::BUILD_FAILED));
            livecodeBuilder.sendMessage(l);
        }
        else if (status == CompilationStatus::NotNeeded)
        {
        }

        livecodeBuilder.sendActivityListUpdate();

        return ThreadPoolJob::jobHasFinished;
    }

private:
    LiveCodeBuilderImpl& livecodeBuilder;
    File fileToCompile;
    String stringToCompile;
    bool isUsingString;
    Array<LiveCodeChange> changesToCompile;
    bool isUsingChanges;
};

//==============================================================================
class CleanAllJob : public ThreadPoolJob
{
public:
    CleanAllJob(LiveCodeBuilderImpl& liveCodeBuilder_)
        : ThreadPoolJob("__clean"),
          livecodeBuilder(liveCodeBuilder_)
    {
    }

    JobStatus runJob() override
    {
        livecodeBuilder.cleanAllFiles();
        livecodeBuilder.sendActivityListUpdate();

        return ThreadPoolJob::jobHasFinished;
    }

private:
    LiveCodeBuilderImpl& livecodeBuilder;
};

//==============================================================================
class RunAppJob : public ThreadPoolJob
{
public:
    RunAppJob(LiveCodeBuilderImpl& liveCodeBuilder_)
        : ThreadPoolJob("__run"),
          livecodeBuilder(liveCodeBuilder_)
    {
    }

    JobStatus runJob() override
    {
        livecodeBuilder.runApp();
        livecodeBuilder.sendActivityListUpdate();

        return ThreadPoolJob::jobHasFinished;
    }

private:
    LiveCodeBuilderImpl& livecodeBuilder;
};

//==============================================================================
class ActivityListUpdateJob : public ThreadPoolJob
{
public:
    ActivityListUpdateJob(LiveCodeBuilderImpl& liveCodeBuilder_)
        : ThreadPoolJob("__activity"),
        livecodeBuilder(liveCodeBuilder_)
    {
    }

    JobStatus runJob() override
    {
        livecodeBuilder.sendActivityListUpdate();
        return ThreadPoolJob::jobHasFinished;
    }

private:
    LiveCodeBuilderImpl& livecodeBuilder;
};

//==============================================================================
LiveCodeBuilderImpl::LiveCodeBuilderImpl(SendMessageFunction sendFunction,
                                         void* userInfo,
                                         const String& projectID,
                                         const String& cacheFolderPath)
    : Thread("LiveCodeBuilder"),
      sendMessageFunction(sendFunction),
      callbackUserInfo(userInfo),
      juceProjectID(projectID),
      juceCacheFolder(cacheFolderPath),
      activitiesPool(1),
      diagOpts(new DiagnosticOptions()),
      diagClient(new DiagnosticReporter(*this, llvm::errs(), &*diagOpts)),
      diagIdentifier(new DiagnosticIDs()),
      diagEngine(diagIdentifier, &*diagOpts, diagClient)
{
    // Initialize native targets
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Set triple to ELF (windows)
    llvm::Triple tripleValue(llvm::sys::getProcessTriple());
    if (tripleValue.isOSBinFormatCOFF())
        tripleValue.setObjectFormat(llvm::Triple::ELF);

    // Create the llvm context
    context = llvm::make_unique<llvm::LLVMContext>();

    // Create the compiler driver
    compilerDriver = llvm::make_unique<Driver>(getExecutablePath("app"), tripleValue.str(), diagEngine);
    compilerDriver->setTitle("clang interpreter");
    compilerDriver->setCheckInputsExist(false);

    // Create the compiler instance
    compilerInstance = llvm::make_unique<CompilerInstance>();
    compilerInstance->createDiagnostics();
    if (! compilerInstance->hasDiagnostics())
        llvm::errs() << "Cannot create compiler diagnostics" << "\n";

    // Cache folder
    if (! juceCacheFolder.exists())
        juceCacheFolder.createDirectory();

    // Search for xcode installation
    File clangPath("/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang");
    if (clangPath.exists() && clangPath.isDirectory())
    {
        DirectoryIterator iter(clangPath, false, "*", File::findDirectories);
        while (iter.next())
        {
            const File includeFolder = iter.getFile().getChildFile("include");
            if (includeFolder.exists())
            {
                clangIncludePath = "-I" + includeFolder.getFullPathName();
            }
        }
    }

    // Start message pump thread
    startThread();
}

LiveCodeBuilderImpl::~LiveCodeBuilderImpl()
{
    activitiesPool.removeAllJobs(true, 5000);

    messageQueue.push(MessageEvents::ExitThread);
    stopThread(10000);
}

//==============================================================================
void LiveCodeBuilderImpl::setBuildInfo(const ValueTree& data)
{
    //LOG(">>>>>>>>>>>>>>>>>>>>> building project");
    //LOG(data.toXmlString());

    // parse data
    systemPath = data.getProperty("systempath").toString().trim();
    userPath = data.getProperty("userpath").toString().trim();

    defines.clear();
    defines.addTokens(data.getProperty("defines").toString().trim(), " ", "");

    extraCompilerFlags.clear();
    extraCompilerFlags.addTokens(data.getProperty("extraCompilerFlags").toString().trim(), " ", "");

    extraDLLs = data.getProperty("extraDLLs").toString().trim();
    juceModulesFolder = data.getProperty("juceModulesFolder").toString().trim();
    utilsCppInclude = data.getProperty("utilsCppInclude").toString().trim();

    // prepare files
    compileUnits.clear();
    userFiles.clear();
    for (int i = 0; i < data.getNumChildren(); i++)
    {
        ValueTree child = data.getChild(i);
        if (child.getType() == MessageTypes::COMPILEUNIT)
        {
            File file(child.getProperty("file"));
            if (file.existsAsFile())
                compileUnits.addIfNotAlreadyThere(file);
        }
        else if (child.getType() == MessageTypes::USERFILE)
        {
            File file(child.getProperty("file"));
            if (file.existsAsFile())
                userFiles.addIfNotAlreadyThere(file);
        }
    }

    // trigger a build project
    sendCompileProject();
}

//==============================================================================
void LiveCodeBuilderImpl::fileUpdated(const File& file, const String& optionalText)
{
    for (int i = 0; i < activitiesPool.getNumJobs(); i++)
    {
        if (ThreadPoolJob* job = activitiesPool.getJob(i))
        {
            if (job->getJobName() == file.getFileName())
            {
                sendActivityListUpdate();
                return;
            }
        }
    }

    activitiesPool.addJob(new CompileJob(*this,
                                         file,
                                         optionalText,
                                         true,
                                         Array<LiveCodeChange>(),
                                         false), true);

    sendActivityListUpdate();
}

//==============================================================================
void LiveCodeBuilderImpl::fileChanged(const File& file, const Array<LiveCodeChange>& changes)
{
    for (int i = 0; i < activitiesPool.getNumJobs(); i++)
    {
        if (ThreadPoolJob* job = activitiesPool.getJob(i))
        {
            if (job->getJobName() == file.getFileName())
            {
                sendActivityListUpdate();
                return;
            }
        }
    }

    activitiesPool.addJob(new CompileJob(*this,
                                         file,
                                         String(),
                                         false,
                                         changes,
                                         true), true);

    sendActivityListUpdate();
}

//==============================================================================
void LiveCodeBuilderImpl::fileReset(const File& file)
{
    // delete cached module

    if (getCacheSourceFile(file).existsAsFile())
        getCacheSourceFile(file).deleteFile();

    if (getCacheBitCodeFile(file).existsAsFile())
        getCacheBitCodeFile(file).deleteFile();

    sendActivityListUpdate();
}

//==============================================================================
void LiveCodeBuilderImpl::reloadComponents()
{
    // TODO
}

//==============================================================================
void LiveCodeBuilderImpl::cleanAll()
{
    activitiesPool.addJob(new CleanAllJob(*this), true);

    sendActivityListUpdate();
}

//==============================================================================
void LiveCodeBuilderImpl::launchApp()
{
    //activitiesPool.addJob(new RunAppJob(*this), true);

    sendActivityListUpdate();

    runApp();
}

void LiveCodeBuilderImpl::runApp()
{
    std::lock_guard<std::mutex> lock(modulesMutex);

    if (modules.size() > 0 && modules.size() == compileUnits.size())
    {
        sendMessage(ValueTree(MessageTypes::LAUNCHED));

        llvm::Module& m = *modules[0];

        // link modules
        for (int i = 1; i < modules.size(); i++)
            llvm::Linker::linkModules(m, std::move(modules[i]));

        // build execution engine
        std::string errorString;
        std::unique_ptr<llvm::ExecutionEngine> executionEngine(createExecutionEngine(std::move(modules[0]), &errorString));
        if (! executionEngine)
        {
            llvm::errs() << "unable to make execution engine: " << errorString << "\n";

            LOG("unable to make execution engine " << String::fromUTF8(errorString.c_str()));
            return;
        }

        executionEngine->finalizeObject();
        executionEngine->runStaticConstructorsDestructors(false);

        llvm::Function* mainFunction = executionEngine->FindFunctionNamed("main");
        if (! mainFunction)
        {
            llvm::errs() << "'main' function not found in module.\n";

            return;
        }

        std::vector<std::string> args;
        args.push_back("app");
        const int result = executionEngine->runFunctionAsMain(mainFunction, args, nullptr);
        if (result != 0)
        {
            llvm::errs() << "Error executing main.\n";

            return;
        }

        executionEngine->runStaticConstructorsDestructors(true);
    }
}

//==============================================================================
void LiveCodeBuilderImpl::pong()
{
    sendMessage(ValueTree(MessageTypes::PING));
}

//==============================================================================
void LiveCodeBuilderImpl::foregroundProcess(bool parentActive)
{
}

//==============================================================================
void LiveCodeBuilderImpl::sendCompileProject()
{
    messageQueue.push(MessageEvents::CompileProject);
}

void LiveCodeBuilderImpl::sendActivityListUpdate()
{
    messageQueue.push(MessageEvents::UpdateActivities);
}

//==============================================================================
void LiveCodeBuilderImpl::run()
{
    while (! threadShouldExit())
    {
        MessageEvents event;
        messageQueue.waitAndPop(event);

        if (threadShouldExit())
            break;

        do
        {
            switch (event)
            {
                case MessageEvents::CompileProject:
                {
                    sendActivityListUpdate();
                    buildProjectIfNeeded();
                    break;
                }

                case MessageEvents::UpdateActivities:
                {
                    StringArray list = activitiesPool.getNamesOfAllJobs(false);
                    StringArray finalList;
                    for (int i = 0; i < list.size(); i++)
                    {
                        String& job = list.getReference(i);
                        if (job.endsWithIgnoreCase(".cpp") || job.endsWithIgnoreCase(".c") || job.endsWithIgnoreCase(".mm"))
                            finalList.add("Compile " + job);
                        else if (! job.startsWith("__"))
                            finalList.add(job);
                    }

                    ValueTree v(MessageTypes::ACTIVITY_LIST);
                    v.setProperty(Ids::list, concatenateListOfStrings(finalList), nullptr);
                    sendMessage(v);

                    break;
                }

                default:
                {
                    break;
                }
            }
        } while (messageQueue.tryAndPop(event));
    }
}

//==============================================================================
CompilationStatus LiveCodeBuilderImpl::compileFileIfNeeded(const File& file,
                                                           const String& string,
                                                           bool useString,
                                                           const Array<LiveCodeChange>& changes,
                                                           bool useChanges,
                                                           String& errorString)
{
    std::lock_guard<std::mutex> lock(modulesMutex);

    errorString = String();

    if (file.getFileExtension().equalsIgnoreCase(".h"))
    {
        // TODO - this will need to rebuild some other files that includes that

        return CompilationStatus::NotNeeded;
    }

    bool fileHasChanged = true;
    File cachedSource(getCacheSourceFile(file));

    if (useString)
    {
        if (cachedSource.existsAsFile())
        {
            String cachedText;

            if (ScopedPointer<InputStream> is = cachedSource.createInputStream())
                cachedText = is->readEntireStreamAsString();

            if (cachedText != string)
                cachedSource.deleteFile();
        }

        if (! cachedSource.existsAsFile())
        {
            if (ScopedPointer<OutputStream> os = cachedSource.createOutputStream())
                os->writeText(string, false, false);
        }
    }
    else
    {
        if (! cachedSource.existsAsFile())
            file.copyFileTo(cachedSource);
    }

    if (useChanges && changes.size())
    {
        String content;

        if (ScopedPointer<InputStream> is = file.createInputStream())
            content = is->readEntireStreamAsString();

        for (int i = 0; i < changes.size(); i++)
            content = content.replaceSection(changes[i].start, changes[i].end - changes[i].start, changes[i].text);

        if (cachedSource.existsAsFile())
            cachedSource.deleteFile();

        if (ScopedPointer<OutputStream> os = cachedSource.createOutputStream())
            os->writeText(content, false, false);
    }

    if (cachedSource.existsAsFile())
    {
        MD5 cachedFileMD5(cachedSource);
        MD5 fileMD5(file);

        if (fileMD5 == cachedFileMD5)
            fileHasChanged = false;
    }

    bool moduleIsAlreadyCompiled = false;
    for (auto&& module : modules)
    {
        if (module && module->getSourceFileName().compare(cachedSource.getFullPathName().toStdString()) == 0)
        {
            moduleIsAlreadyCompiled = true;
            break;
        }
    }

    if (! fileHasChanged && getCacheBitCodeFile(file).existsAsFile())
    {
        llvm::SMDiagnostic diag;
        if (ModulePtr module = llvm::parseIRFile(getCacheBitCodeFile(file).getFullPathName().toRawUTF8(), diag, *context))
        {
            module->setSourceFileName(cachedSource.getFullPathName().toRawUTF8());
            modules.push_back(std::move(module));

            return CompilationStatus::NotNeeded;
        }
    }

    if (fileHasChanged || ! moduleIsAlreadyCompiled)
    {
        LOG("Compiling " << file.getFullPathName());

        if (ModulePtr module = compileFile(file))
        {
            if (getCacheBitCodeFile(file).existsAsFile())
                getCacheBitCodeFile(file).deleteFile();

            std::error_code errorCode;
            llvm::raw_fd_ostream outputFile(getCacheBitCodeFile(file).getFullPathName().toRawUTF8(), errorCode, llvm::sys::fs::F_None);
            llvm::WriteBitcodeToFile(module.get(), outputFile);
            outputFile.flush();

            modules.push_back(std::move(module));

            return CompilationStatus::Ok;
        }
        else
        {
            return CompilationStatus::Error;
        }
    }

    return CompilationStatus::NotNeeded;
}

//==============================================================================
void LiveCodeBuilderImpl::buildProjectIfNeeded()
{
    std::lock_guard<std::mutex> lock(modulesMutex);

    int numberOfFilesToCompile = 0;

    // compile units
    for (int i = 0; i < compileUnits.size(); i++)
    {
        File& file = compileUnits.getReference(i);
        bool moduleFound = false;

        for (auto&& module : modules)
        {
            if (module->getSourceFileName().compare(file.getFullPathName().toStdString()) == 0)
            {
                moduleFound = true;
                break;
            }
        }

        if (! moduleFound)
        {
            fileChanged(file);

            ++numberOfFilesToCompile;
        }
    }

    if (compileUnits.size() > 0 && numberOfFilesToCompile == 0)
    {
        activitiesPool.addJob(new ActivityListUpdateJob(*this), true);
    }

    sendActivityListUpdate();
}

//==============================================================================
void LiveCodeBuilderImpl::cleanAllFiles()
{
    {
    std::lock_guard<std::mutex> lock(modulesMutex);
    modules.clear();

    DirectoryIterator it(juceCacheFolder, false);
    while (it.next())
    {
        File file = it.getFile();
        file.deleteRecursively();
    }

    compilerInstance->clearOutputFiles(true);
    }
}

//==============================================================================
void LiveCodeBuilderImpl::sendMessage(const ValueTree& tree)
{
    if (sendMessageFunction != nullptr)
    {
        MemoryOutputStream out;
        tree.writeToStream(out);
        MemoryBlock mb = out.getMemoryBlock();

        sendMessageFunction(callbackUserInfo, mb.getData(), mb.getSize());
    }
}

//==============================================================================
File LiveCodeBuilderImpl::getCacheSourceFile(const File& file) const
{
    return juceCacheFolder.getChildFile(file.getFileName());
}

File LiveCodeBuilderImpl::getCacheBitCodeFile(const File& file) const
{
    return juceCacheFolder.getChildFile(file.getFileNameWithoutExtension()).withFileExtension(".bc");
}

DiagnosticReporter* LiveCodeBuilderImpl::getDiagnostics()
{
    return diagClient;
}

//==============================================================================
ModulePtr LiveCodeBuilderImpl::compileFile(const File& file)
{
    std::unique_ptr<CodeGenAction> codeGenAction(generateCode(file));
    if (codeGenAction)
        return std::move(codeGenAction->takeModule());
    return ModulePtr();
}

//==============================================================================
std::unique_ptr<CodeGenAction> LiveCodeBuilderImpl::generateCode(const File& file)
{
    // Create compiler invocation
    std::unique_ptr<CompilerInvocation> compilerInvocation(createCompilerInvocation(file));
    if (! compilerInvocation)
    {
        return std::unique_ptr<CodeGenAction>();
    }

    // Set the invocation to the instance
    compilerInstance->setInvocation(compilerInvocation.release());

    // Emit a codegen
    std::unique_ptr<CodeGenAction> codeGenAction(new EmitLLVMOnlyAction(context.get()));
    if (! compilerInstance->ExecuteAction(*codeGenAction))
    {
        return std::unique_ptr<CodeGenAction>();
    }

    return codeGenAction;
}

//==============================================================================
std::unique_ptr<CompilerInvocation> LiveCodeBuilderImpl::createCompilerInvocation(const File& file)
{
    std::vector<const char*> args;

    // compiler
    if (file.getFileExtension().equalsIgnoreCase(".c"))
    {
        args.push_back("clang");
    }
    else
    {
        args.push_back("clang++");
        args.push_back("-std=c++11");
        //args.push_back("-stdlib=libstdc++");
    }

    // syntax only
    args.push_back("-fsyntax-only");
    args.push_back("-fno-use-cxa-atexit");
    //args.push_back("-O3");

    // base includes
    args.push_back("-I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1");
    args.push_back(clangIncludePath.toRawUTF8());
    args.push_back("-I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include");
    args.push_back("-I/usr/local/include");
    args.push_back("-I/usr/include");
    args.push_back("-I/System/Library/Frameworks");
    args.push_back("-I/Library/Frameworks");

    // compile flags
    args.push_back("-c");
    for (int i = 0; i < extraCompilerFlags.size(); i++)
        args.push_back(extraCompilerFlags[i].toRawUTF8());

    // defines
    StringArray definesTemp;
    for (int i = 0; i < defines.size(); i++) {
        definesTemp.add("-D" + defines[i]);
    }

    //args.push_back("-DDEBUG=1");
    args.push_back("-DNDEBUG=1");
    args.push_back("-DJUCE_CHECK_MEMORY_LEAKS=1");
    args.push_back("-DJUCE_NO_COMPILER_THREAD_LOCAL=1");
    for (int i = 0; i < definesTemp.size(); i++) {
        args.push_back(definesTemp[i].toRawUTF8());
    }

    // include paths
    String filePathTemp = "-I" + file.getParentDirectory().getFullPathName();
    args.push_back(filePathTemp.toRawUTF8());

    String systemPathTemp = "-I" + systemPath;
    if (systemPath.isNotEmpty())
        args.push_back(systemPathTemp.toRawUTF8());

    String userPathTemp = "-I" + userPath;
    if (userPath.isNotEmpty())
        args.push_back(userPathTemp.toRawUTF8());

    // frameworks
    /*
    const char* frameworks[] = {
        "Accelerate",
        "AudioToolbox",
        "Carbon",
        "Cocoa",
        "CoreAudio",
        "CoreMIDI",
        "IOKit",
        "OpenGL",
        "QTKit",
        "QuartzCore",
        "WebKit"
    };

    for (int i = 0; i < sizeof(frameworks) / sizeof(frameworks[0]); i++)
    {
        args.add("-framework");
        args.add(frameworks[i]);
    }
    */

    // file to compile
    String filePath = getCacheSourceFile(file).getFullPathName();
    args.push_back(filePath.toRawUTF8());

    // starts compilation
    Compilation* compilation = compilerDriver->BuildCompilation(args);
    if (! compilation)
        return std::unique_ptr<CompilerInvocation>();

    // We expect to get back exactly one command job, if we didn't something failed.
    const driver::JobList& jobs = compilation->getJobs();
    if (jobs.size() != 1 || ! isa<driver::Command>(*jobs.begin()))
    {
        SmallString<256> msg;
        llvm::raw_svector_ostream stream(msg);
        jobs.Print(stream, "; ", true);
        diagEngine.Report(diag::err_fe_expected_compiler_job) << stream.str();

        return std::unique_ptr<CompilerInvocation>();
    }

    const driver::Command& cmd = cast<driver::Command>(*jobs.begin());
    if (llvm::StringRef(cmd.getCreator().getName()) != "clang")
    {
        diagEngine.Report(diag::err_fe_expected_clang_command);

        return std::unique_ptr<CompilerInvocation>();
    }

    const driver::ArgStringList& ccArgs = cmd.getArguments();

    auto compilerInvocation = llvm::make_unique<CompilerInvocation>();
    CompilerInvocation::CreateFromArgs(*compilerInvocation,
                                       const_cast<const char **>(ccArgs.data()),
                                       const_cast<const char **>(ccArgs.data()) +
                                       ccArgs.size(),
                                       diagEngine);

    // Show the invocation, with -v.
    if (compilerInvocation->getHeaderSearchOpts().Verbose)
    {
        llvm::errs() << "clang invocation:\n";
        jobs.Print(llvm::errs(), "\n", true);
        llvm::errs() << "\n";
    }

    return compilerInvocation;
}

//==============================================================================
std::unique_ptr<llvm::ExecutionEngine> LiveCodeBuilderImpl::createExecutionEngine(ModulePtr module, std::string* errorString)
{
    return std::unique_ptr<llvm::ExecutionEngine>(llvm::EngineBuilder(std::move(module))
                                                  .setEngineKind(llvm::EngineKind::JIT)
                                                  .setMCJITMemoryManager(std::unique_ptr<llvm::SectionMemoryManager>(new llvm::SectionMemoryManager()))
                                                  .setVerifyModules(true)
                                                  .setErrorStr(errorString)
                                                  .create());
}

//==============================================================================
// This function isn't referenced outside its translation unit, but it
// can't use the "static" keyword because its address is used for
// GetMainExecutable (since some platforms don't support taking the
// address of main, and some platforms can't implement GetMainExecutable
// without being given the address of a function in the main executable).
std::string getExecutablePath(const char* Argv0)
{
    // This just needs to be some symbol in the binary; C++ doesn't
    // allow taking the address of ::main however.
    void* MainAddr = (void*) (intptr_t) getExecutablePath;
    return llvm::sys::fs::getMainExecutable(Argv0, MainAddr);
}

//==============================================================================
extern "C"
{
    int tigetnum(char*capname) { return 0; }
    int setupterm(char *term, int fildes, int *errret) { return 0; }
    void* set_curterm(void* newterminal) { return nullptr; }
    int del_curterm(void* terminal) { return 0; }
}
