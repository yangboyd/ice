// **********************************************************************
//
// Copyright (c) 2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <IceUtil/Options.h>
#include <IcePatch2/Util.h>

#ifdef _WIN32
#   include <direct.h>
#endif

using namespace std;
using namespace Ice;
using namespace IcePatch2;

struct FileInfoPathLess: public binary_function<const FileInfo&, const FileInfo&, bool>
{
    bool
    operator()(const FileInfo& lhs, const FileInfo& rhs)
    {
	return lhs.path < rhs.path;
    }
};

struct IFileInfoPathEqual: public binary_function<const FileInfo&, const FileInfo&, bool>
{
    bool
    operator()(const FileInfo& lhs, const FileInfo& rhs)
    {
	if(lhs.path.size() != rhs.path.size())
	{
	    return false;
	}

	for(string::size_type i = 0; i < lhs.path.size(); ++i)
	{
	    if(::tolower(lhs.path[i]) != ::tolower(rhs.path[i]))
	    {
		return false;
	    }
	}

	return true;
    }
};

struct IFileInfoPathLess: public binary_function<const FileInfo&, const FileInfo&, bool>
{
    bool
    operator()(const FileInfo& lhs, const FileInfo& rhs)
    {
	for(string::size_type i = 0; i < lhs.path.size() && i < rhs.path.size(); ++i)
	{
	    if(::tolower(lhs.path[i]) < ::tolower(rhs.path[i]))
	    {
		return true;
	    }
	    else if(::tolower(lhs.path[i]) > ::tolower(rhs.path[i]))
	    {
		return false;
	    }
	}
	return lhs.path.size() < rhs.path.size();
    }
};

class CalcCB : public GetFileInfoSeqCB
{
public:

    virtual bool
    remove(const string& path)
    {
	cout << "removing: " << path << endl;
	return true;
    }

    virtual bool
    checksum(const string& path)
    {
	cout << "checksum: " << path << endl;
	return true;
    }

    virtual bool
    compress(const string& path)
    {
	cout << "compress: " << path << endl;
	return true;
    }
};

void
usage(const char* appName)
{
    cerr << "Usage: " << appName << " [options] DIR [FILES...]\n";
    cerr <<     
        "Options:\n"
        "-h, --help              Show this message.\n"
        "-v, --version           Display the Ice version.\n"
        "-z, --compress          Always compress files.\n"
        "-Z, --no-compress       Never compress files.\n"
        "-i, --case-insensitive  Files must not differ in case only.\n"
        "-V, --verbose           Verbose mode.\n"
        ;
}

int
main(int argc, char* argv[])
{
    string dataDir;
    StringSeq fileSeq;
    int compress = 1;
    bool verbose;
    bool caseInsensitive;

    IceUtil::Options opts;
    opts.addOpt("h", "help");
    opts.addOpt("v", "version");
    opts.addOpt("z", "compress");
    opts.addOpt("Z", "no-compress");
    opts.addOpt("V", "verbose");
    opts.addOpt("i", "case-insensitive");
    
    vector<string> args;
    try
    {
    	args = opts.parse(argc, argv);
    }
    catch(const IceUtil::Options::BadOpt& e)
    {
        cerr << e.reason << endl;
	usage(argv[0]);
	return EXIT_FAILURE;
    }

    if(opts.isSet("h") || opts.isSet("help"))
    {
	usage(argv[0]);
	return EXIT_SUCCESS;
    }
    if(opts.isSet("v") || opts.isSet("version"))
    {
	cout << ICE_STRING_VERSION << endl;
	return EXIT_SUCCESS;
    }
    bool doCompress = opts.isSet("z") || opts.isSet("compress");
    bool dontCompress = opts.isSet("Z") || opts.isSet("no-compress");
    if(doCompress && dontCompress)
    {
        cerr << argv[0] << ": only one of -z and -Z are mutually exclusive" << endl;
	usage(argv[0]);
	return EXIT_FAILURE;
    }
    if(doCompress)
    {
        compress = 2;
    }
    else if(dontCompress)
    {
        compress = 0;
    }
    verbose = opts.isSet("V") || opts.isSet("verbose");
    caseInsensitive = opts.isSet("i") || opts.isSet("case-insensitive");

    if(args.empty())
    {
        cerr << argv[0] << ": no data directory specified" << endl;
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    try
    {
	//
	// Make working directory the data directory *before* calling normalize() for
	// for the first time (because normalize caches the current working directory).
	//
	if(chdir(args[0].c_str()) != 0)
	{
	    string msg = "cannot change working directory to `" + args[0] + "': " + lastError();
	    throw msg;
	}
	dataDir = normalize(".");
	string dataDirWithSlash = dataDir + "/";

	for(vector<string>::size_type i = 1; i < args.size(); ++i)
	{
	    fileSeq.push_back(normalize(args[i]));
	}

	StringSeq::iterator p;
	for(p = fileSeq.begin(); p != fileSeq.end(); ++p)
	{
	    if(p->compare(0, dataDirWithSlash.size(), dataDirWithSlash) != 0)
	    {
		throw "`" + *p + "' is not a path in `" + dataDir + "'";
	    }
	    
	    p->erase(0, dataDirWithSlash.size());
	    *p = "./" + *p;
	}
    
	FileInfoSeq infoSeq;
	    
	if(fileSeq.empty())
	{
	    CalcCB calcCB;
	    getFileInfoSeq(".", compress, verbose ? &calcCB : 0, infoSeq);
	}
	else
	{
	    loadFileInfoSeq(".", infoSeq);

	    for(p = fileSeq.begin(); p != fileSeq.end(); ++p)
	    {
		FileInfoSeq partialInfoSeq;

		CalcCB calcCB;
		getFileInfoSeq(*p, compress, verbose ? &calcCB : 0, partialInfoSeq);

		FileInfoSeq newInfoSeq;
		newInfoSeq.reserve(infoSeq.size());

		set_difference(infoSeq.begin(),
			       infoSeq.end(),
			       partialInfoSeq.begin(),
			       partialInfoSeq.end(),
			       back_inserter(newInfoSeq),
			       FileInfoPathLess());

		infoSeq.swap(newInfoSeq);

		newInfoSeq.clear();
		newInfoSeq.reserve(infoSeq.size() + partialInfoSeq.size());

		set_union(infoSeq.begin(),
			  infoSeq.end(),
			  partialInfoSeq.begin(),
			  partialInfoSeq.end(),
			  back_inserter(newInfoSeq),
			  FileInfoPathLess());

		infoSeq.swap(newInfoSeq);
	    }
	}

	if(caseInsensitive)
	{
	    FileInfoSeq newInfoSeq = infoSeq;
	    sort(newInfoSeq.begin(), newInfoSeq.end(), IFileInfoPathLess());

	    string ex;
	    FileInfoSeq::iterator p = newInfoSeq.begin();
	    while((p = adjacent_find(p, newInfoSeq.end(), IFileInfoPathEqual())) != newInfoSeq.end())
	    {
		do
		{
		    ex += '\n' + dataDir + '/' + p->path;
		    ++p;
		}
		while(p < newInfoSeq.end() && IFileInfoPathEqual()(*(p - 1), *p));
	    }

	    if(!ex.empty())
	    {
		ex = "duplicate files:" + ex;
		throw ex;
	    }
	}

	saveFileInfoSeq(dataDir, infoSeq);
    }
    catch(const string& ex)
    {
        cerr << argv[0] << ": " << ex << endl;
        return EXIT_FAILURE;
    }
    catch(const char* ex)
    {
        cerr << argv[0] << ": " << ex << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
