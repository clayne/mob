#include "pch.h"
#include "conf.h"
#include "utility.h"
#include "context.h"
#include "process.h"
#include "tools/tools.h"

namespace mob
{

std::string master_ini_filename()
{
	return "mob.ini";
}

conf::task_map conf::map_;
int conf::output_log_level_ = 3;
int conf::file_log_level_ = 5;


std::string conf::get_global(const std::string& section, const std::string& key)
{
	auto global = map_.find("");
	MOB_ASSERT(global != map_.end());

	auto sitor = global->second.find(section);
	if (sitor == global->second.end())
	{
		gcx().bail_out(context::conf,
			"conf section '{}' doesn't exist", section);
	}

	auto kitor = sitor->second.find(key);
	if (kitor == sitor->second.end())
	{
		gcx().bail_out(context::conf,
			"key '{}' not found in section '{}'", key, section);
	}

	return kitor->second;
}

void conf::set_global(
	const std::string& section,
	const std::string& key, const std::string& value)
{
	auto global = map_.find("");
	MOB_ASSERT(global != map_.end());

	auto sitor = global->second.find(section);
	if (sitor == global->second.end())
	{
		gcx().bail_out(context::conf,
			"conf section '{}' doesn't exist", section);
	}

	auto kitor = sitor->second.find(key);
	if (kitor == sitor->second.end())
	{
		gcx().bail_out(context::conf,
			"key '{}' not found in section '{}'", key, section);
	}

	kitor->second = value;
}

void conf::add_global(
	const std::string& section,
	const std::string& key, const std::string& value)
{
	map_[""][section][key] = value;
}

std::string conf::get_for_task(
	const std::vector<std::string>& task_names,
	const std::string& section, const std::string& key)
{
	task_map::iterator task = map_.end();

	for (auto&& tn : task_names)
	{
		task = map_.find(tn);
		if (task != map_.end())
			break;
	}

	if (task == map_.end())
		return get_global(section, key);

	auto sitor = task->second.find(section);
	if (sitor == task->second.end())
		return get_global(section, key);

	auto kitor = sitor->second.find(key);
	if (kitor == sitor->second.end())
		return get_global(section, key);

	return kitor->second;
}

void conf::set_for_task(
	const std::string& task_name, const std::string& section,
	const std::string& key, const std::string& value)
{
	// make sure it exists, will throw if it doesn't
	get_global(section, key);

	map_[task_name][section][key] = value;
}

bool conf::prebuilt_by_name(const std::string& task)
{
	std::istringstream iss(get_global("prebuilt", task));
	bool b;
	iss >> std::boolalpha >> b;
	return b;
}

fs::path conf::path_by_name(const std::string& name)
{
	return get_global("paths", name);
}

std::string conf::version_by_name(const std::string& name)
{
	return get_global("versions", name);
}

fs::path conf::tool_by_name(const std::string& name)
{
	return get_global("tools", name);
}

std::string conf::global_by_name(const std::string& name)
{
	return get_global("global", name);
}

bool conf::bool_global_by_name(const std::string& name)
{
	std::istringstream iss(get_global("global", name));
	bool b;
	iss >> std::boolalpha >> b;
	return b;
}

std::string conf::option_by_name(
	const std::vector<std::string>& task_names, const std::string& name)
{
	return get_for_task(task_names, "options", name);
}

void conf::set_output_log_level(const std::string& s)
{
	if (s.empty())
		return;

	try
	{
		const auto i = std::stoi(s);

		if (i < 0 || i > 6)
			gcx().bail_out(context::generic, "bad output log level {}", i);

		output_log_level_ = i;
	}
	catch(std::exception&)
	{
		gcx().bail_out(context::generic, "bad output log level {}", s);
	}
}

void conf::set_file_log_level(const std::string& s)
{
	if (s.empty())
		return;

	try
	{
		const auto i = std::stoi(s);
		if (i < 0 || i > 6)
			gcx().bail_out(context::generic, "bad file log level {}", i);

		file_log_level_ = i;
	}
	catch(std::exception&)
	{
		gcx().bail_out(context::generic, "bad file log level {}", s);
	}
}

std::vector<std::string> conf::format_options()
{
	std::size_t longest_task = 0;
	std::size_t longest_section = 0;
	std::size_t longest_key = 0;

	for (auto&& [t, ss] : map_)
	{
		longest_task = std::max(longest_task, t.size());

		for (auto&& [s, kv] : ss)
		{
			longest_section = std::max(longest_section, s.size());

			for (auto&& [k, v] : kv)
				longest_key = std::max(longest_key, k.size());
		}
	}

	std::vector<std::string> lines;

	for (auto&& [t, ss] : map_)
	{
		for (auto&& [s, kv] : ss)
		{
			for (auto&& [k, v] : kv)
			{
				lines.push_back(
					pad_right(t, longest_task) + "  " +
					pad_right(s, longest_section) + "  " +
					pad_right(k, longest_key) + " = " + v);
			}
		}
	}

	return lines;
}


struct parsed_option
{
	std::string section, key, value;
};

parsed_option parse_option(const std::string& s)
{
	const auto slash = s.find("/");
	if (slash == std::string::npos)
	{
		gcx().bail_out(context::conf,
			"bad option {}, must be section/key=value", s);
	}

	const auto equal = s.find("=", slash);
	if (slash == std::string::npos)
	{
		gcx().bail_out(context::conf,
			"bad option {}, must be section/key=value", s);
	}

	std::string section = s.substr(0, slash);
	std::string key = s.substr(slash + 1, equal - slash - 1);
	std::string value = s.substr(equal + 1);

	return {section, key, value};
}

bool try_parts(fs::path& check, const std::vector<std::string>& parts)
{
	for (std::size_t i=0; i<parts.size(); ++i)
	{
		fs::path p = check;

		for (std::size_t j=i; j<parts.size(); ++j)
			p /= parts[j];

		gcx().trace(context::conf, "trying parts {}", p);

		if (fs::exists(p))
		{
			check = p;
			return true;
		}
	}

	return false;
}

fs::path find_root_impl()
{
	gcx().trace(context::conf, "looking for root directory");

	fs::path p = fs::current_path();

	if (try_parts(p, {"..", "..", "..", "third-party"}))
		return p;

	gcx().bail_out(context::conf, "root directory not found");
}

fs::path find_root()
{
	const auto p = find_root_impl().parent_path();

	gcx().trace(context::conf, "found root directory at {}", p);

	return p;
}

fs::path find_in_root(const fs::path& file)
{
	static fs::path root = find_root();

	fs::path p = root / file;
	if (!fs::exists(p))
		gcx().bail_out(context::conf, "{} not found", p);

	gcx().trace(context::conf, "found {}", p);
	return p;
}


fs::path find_third_party_directory()
{
	static fs::path path = find_in_root("third-party");
	return path;
}


fs::path find_in_path(const std::string& exe)
{
	const std::wstring wexe = utf8_to_utf16(exe);

	const std::size_t size = MAX_PATH;
	wchar_t buffer[size + 1] = {};

	if (SearchPathW(nullptr, wexe.c_str(), nullptr, size, buffer, nullptr))
		return buffer;
	else
		return {};
}

bool find_qmake(fs::path& check)
{
	// try Qt/Qt5.14.2/msvc*/bin/qmake.exe
	if (try_parts(check, {
		"Qt",
		"Qt" + qt::version(),
		"msvc" + qt::vs_version() + "_64",
		"bin",
		"qmake.exe"}))
	{
		return true;
	}

	// try Qt/5.14.2/msvc*/bin/qmake.exe
	if (try_parts(check, {
		"Qt",
		qt::version(),
		"msvc" + qt::vs_version() + "_64",
		"bin",
		"qmake.exe"}))
	{
		return true;
	}

	return false;
}

bool try_qt_location(fs::path& check)
{
	if (!find_qmake(check))
		return false;

	check = fs::absolute(check.parent_path() / "..");
	return true;
}

fs::path find_qt()
{
	fs::path p = conf::path_by_name("qt_install");

	if (!p.empty())
	{
		p = fs::absolute(p);

		if (!try_qt_location(p))
			gcx().bail_out(context::conf, "no qt install in {}", p);

		return p;
	}


	std::vector<fs::path> locations =
	{
		paths::pf_x64(),
		"C:\\",
		"D:\\"
	};

	// look for qmake, which is in %qt%/version/msvc.../bin
	fs::path qmake = find_in_path("qmake.exe");
	if (!qmake.empty())
		locations.insert(locations.begin(), qmake.parent_path() / "../../");

	// look for qtcreator.exe, which is in %qt%/Tools/QtCreator/bin
	fs::path qtcreator = find_in_path("qtcreator.exe");
	if (!qtcreator.empty())
		locations.insert(locations.begin(), qtcreator.parent_path() / "../../../");

	for (fs::path loc : locations)
	{
		loc = fs::absolute(loc);
		if (try_qt_location(loc))
			return loc;
	}

	gcx().bail_out(context::conf, "can't find qt install");
}

void validate_qt()
{
	fs::path p = qt::installation_path();

	if (!try_qt_location(p))
		gcx().bail_out(context::conf, "qt path {} doesn't exist", p);

	conf::set_global("paths", "qt_install", path_to_utf8(p));
}

fs::path get_known_folder(const GUID& id)
{
	wchar_t* buffer = nullptr;
	const auto r = ::SHGetKnownFolderPath(id, 0, 0, &buffer);

	if (r != S_OK)
		return {};

	fs::path p = buffer;
	::CoTaskMemFree(buffer);

	return p;
}

fs::path find_program_files_x86()
{
	fs::path p = get_known_folder(FOLDERID_ProgramFilesX86);

	if (p.empty())
	{
		const auto e = GetLastError();

		p = fs::path(R"(C:\Program Files (x86))");

		gcx().warning(context::conf,
			"failed to get x86 program files folder, defaulting to {}, {}",
			p, error_message(e));
	}
	else
	{
		gcx().trace(context::conf, "x86 program files is {}", p);
	}

	return p;
}

fs::path find_program_files_x64()
{
	fs::path p = get_known_folder(FOLDERID_ProgramFilesX64);

	if (p.empty())
	{
		const auto e = GetLastError();

		p = fs::path(R"(C:\Program Files)");

		gcx().warning(context::conf,
			"failed to get x64 program files folder, defaulting to {}, {}",
			p, error_message(e));
	}
	else
	{
		gcx().trace(context::conf, "x64 program files is {}", p);
	}

	return p;
}

fs::path find_temp_dir()
{
	const std::size_t buffer_size = MAX_PATH + 2;
	wchar_t buffer[buffer_size] = {};

	if (GetTempPathW(static_cast<DWORD>(buffer_size), buffer) == 0)
	{
		const auto e = GetLastError();
		gcx().bail_out(context::conf, "can't get temp path", error_message(e));
	}

	fs::path p(buffer);
	gcx().trace(context::conf, "temp dir is {}", p);

	return p;
}

fs::path find_vs()
{
	if (conf::dry())
		return vs::vswhere();

	auto p = process()
		.binary(vs::vswhere())
		.arg("-prerelease")
		.arg("-version", vs::version())
		.arg("-property", "installationPath")
		.stdout_flags(process::keep_in_string)
		.stderr_flags(process::inherit);

	p.run();
	p.join();

	if (p.exit_code() != 0)
		gcx().bail_out(context::conf, "vswhere failed");

	fs::path path = trim_copy(p.stdout_string());

	if (!fs::exists(path))
	{
		gcx().bail_out(context::conf,
			"the path given by vswhere doesn't exist: {}", path);
	}

	return path;
}

bool try_vcvars(fs::path& bat)
{
	if (!fs::exists(bat))
		return false;

	bat = fs::canonical(fs::absolute(bat));
	return true;
}

void find_vcvars()
{
	fs::path bat = conf::tool_by_name("vcvars");

	if (conf::dry())
	{
		if (bat.empty())
			bat = "vcvars.bat";

		return;
	}
	else
	{
		if (bat.empty())
		{
			bat = vs::installation_path()
				/ "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

			if (!try_vcvars(bat))
				gcx().bail_out(context::conf, "vcvars not found at {}", bat);
		}
		else
		{
			if (!try_vcvars(bat))
				gcx().bail_out(context::conf, "vcvars not found at {}", bat);
		}
	}

	conf::set_global("tools", "vcvars", path_to_utf8(bat));
	gcx().trace(context::conf, "using vcvars at {}", bat);
}


void ini_error(const fs::path& ini, std::size_t line, const std::string& what)
{
	gcx().bail_out(context::conf,
		"{}:{}: {}",
		ini.filename(), (line + 1), what);
}

fs::path find_master_ini()
{
	auto p = fs::current_path();

	if (try_parts(p, {"..", "..", "..", master_ini_filename()}))
		return fs::canonical(p);

	gcx().bail_out(context::conf,
		"can't find master ini {}", master_ini_filename());
}

std::vector<std::string> read_ini(const fs::path& ini)
{
	std::ifstream in(ini);

	std::vector<std::string> lines;

	for (;;)
	{
		std::string line;
		std::getline(in, line);
		trim(line);

		if (!in)
			break;

		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;

		lines.push_back(std::move(line));
	}

	if (in.bad())
		gcx().bail_out(context::conf, "failed to read ini {}", ini);

	return lines;
}

void parse_section(
	const fs::path& ini, std::size_t& i,
	const std::vector<std::string>& lines,
	const std::string& task, const std::string& section, bool add)
{
	++i;

	for (;;)
	{
		if (i >= lines.size() || lines[i][0] == '[')
			break;

		const auto& line = lines[i];

		const auto sep = line.find("=");
		if (sep == std::string::npos)
			ini_error(ini, i, "bad line '" + line + "'");

		const std::string k = trim_copy(line.substr(0, sep));
		const std::string v = trim_copy(line.substr(sep + 1));

		if (k.empty())
			ini_error(ini, i, "bad line '" + line + "'");

		if (task.empty())
		{
			if (add)
				conf::add_global(section, k, v);
			else
				conf::set_global(section, k, v);
		}
		else
		{
			conf::set_for_task(task, section, k, v);
		}

		++i;
	}
}

void parse_ini(const fs::path& ini, bool add)
{
	gcx().debug(context::conf, "using ini at {}", ini);

	const auto lines = read_ini(ini);
	std::size_t i = 0;

	for (;;)
	{
		if (i >= lines.size())
			break;

		const auto& line = lines[i];

		if (line.starts_with("[") && line.ends_with("]"))
		{
			const std::string s = line.substr(1, line.size() - 2);

			std::string task, section;

			const auto slash = s.find("/");

			if (slash == std::string::npos)
			{
				section = s;
			}
			else
			{
				task = s.substr(0, slash);
				section = s.substr(slash +1 );
			}

			parse_section(ini, i, lines, task, section, add);
		}
		else
		{
			ini_error(ini, i, "bad line '" + line + "'");
		}
	}
}

bool check_missing_options()
{
	if (conf::mo_org({""}).empty())
	{
		u8cerr
			<< "missing mo_org; either specify it the [options] section of "
			<< "the ini or pass '-s options/mo_org=something'\n";

		return false;
	}

	if (conf::mo_branch({""}).empty())
	{
		u8cerr
			<< "missing mo_branch; either specify it the [options] section of "
			<< "the ini or pass '-s options/mo_org=something'\n";

		return false;
	}

	if (paths::prefix().empty())
	{
		u8cerr
			<< "missing prefix; either specify it the [paths] section of "
			<< "the ini or pass '-d path'\n";

		return false;
	}

	return true;
}

template <class F>
void set_path_if_empty(const std::string& k, F&& f)
{
	fs::path p = conf::get_global("paths", k);

	if (p.empty())
	{
		if constexpr (std::is_same_v<fs::path, std::decay_t<decltype(f)>>)
			p = f;
		else
			p = f();
	}

	p = fs::absolute(p);

	if (!conf::dry())
	{
		if (!fs::exists(p))
			gcx().bail_out(context::conf, "path {} not found", p);

		p = fs::canonical(p);
	}

	conf::set_global("paths", k, path_to_utf8(p));
}

void make_canonical_path(
	const std::string& key,
	const fs::path& default_parent, const std::string& default_dir)
{
	fs::path p = conf::path_by_name(key);

	if (p.empty())
	{
		p = default_parent / default_dir;
	}
	else
	{
		if (p.is_relative())
			p = default_parent / p;
	}

	if (!conf::dry())
		p = fs::weakly_canonical(fs::absolute(p));

	conf::set_global("paths", key, path_to_utf8(p));
}

void set_special_options()
{
	conf::set_output_log_level(conf::get_global("global", "output_log_level"));
	conf::set_file_log_level(conf::get_global("global", "file_log_level"));
}

std::vector<fs::path> find_inis(const std::vector<fs::path>& inis_from_cl)
{
	const auto master = find_master_ini();

	std::vector<fs::path> v;

	for (auto&& e : fs::directory_iterator(master.parent_path()))
	{
		const auto p = e.path();

		if (path_to_utf8(p.extension()) != ".ini")
			continue;

		if (p.filename() == master.filename())
			continue;

		v.push_back(p);
	}

	std::sort(v.begin(), v.end());
	v.insert(v.begin(), master);

	for (auto&& p : inis_from_cl)
	{
		if (!fs::exists(p))
		{
			u8cerr << "ini " << p << " not found\n";
			throw bailed();
		}

		bool found = false;

		for (auto itor=v.begin(); itor!=v.end(); ++itor)
		{
			if (fs::equivalent(p, *itor))
			{
				found = true;
				v.erase(itor);
				v.push_back(p);
				break;
			}
		}

		if (!found)
			v.push_back(p);
	}

	return v;
}

void init_options(
	const std::vector<fs::path>& inis_from_cl, bool auto_detection,
	const std::vector<std::string>& opts)
{
	std::vector<fs::path> inis;

	if (auto_detection)
		inis = find_inis(inis_from_cl);
	else
		inis = inis_from_cl;

	MOB_ASSERT(!inis.empty());

	bool add = true;
	for (auto&& ini : inis)
	{
		parse_ini(ini, add);
		add = false;
	}

	if (!opts.empty())
	{
		gcx().debug(context::conf, "overriding from command line:");

		for (auto&& o : opts)
		{
			const auto po = parse_option(o);
			conf::set_global(po.section, po.key, po.value);
		}
	}

	set_special_options();
	context::set_log_file(conf::log_file());

	gcx().debug(context::conf, "using inis in order:");
	for (auto&& ini : inis)
		gcx().debug(context::conf, "  . {}", ini);

	set_path_if_empty("third_party", find_third_party_directory);
	this_env::prepend_to_path(paths::third_party() / "bin");

	set_path_if_empty("pf_x86",     find_program_files_x86);
	set_path_if_empty("pf_x64",     find_program_files_x64);
	set_path_if_empty("vs",         find_vs);
	set_path_if_empty("qt_install", find_qt);
	set_path_if_empty("temp_dir",   find_temp_dir);
	set_path_if_empty("patches",    find_in_root("patches"));
	set_path_if_empty("licenses",   find_in_root("licenses"));
	set_path_if_empty("qt_bin",     qt::installation_path() / "bin");

	find_vcvars();
	validate_qt();

	if (!paths::prefix().empty())
		make_canonical_path("prefix",           fs::current_path(), "");

	make_canonical_path("cache",            paths::prefix(), "downloads");
	make_canonical_path("build",            paths::prefix(), "build");
	make_canonical_path("install",          paths::prefix(), "install");
	make_canonical_path("install_bin",      paths::install(), "bin");
	make_canonical_path("install_libs",     paths::install(), "libs");
	make_canonical_path("install_pdbs",     paths::install(), "pdb");
	make_canonical_path("install_dlls",     paths::install_bin(), "dlls");
	make_canonical_path("install_loot",     paths::install_bin(), "loot");
	make_canonical_path("install_plugins",  paths::install_bin(), "plugins");
	make_canonical_path("install_licenses", paths::install_bin(), "licenses");

	make_canonical_path(
		"install_pythoncore",
		paths::install_bin(), "pythoncore");

	make_canonical_path(
		"install_stylesheets",
		paths::install_bin(), "stylesheets");
}

bool verify_options()
{
	return check_missing_options();
}

void log_options()
{
	for (auto&& line : conf::format_options())
		gcx().trace(context::conf, "{}", line);
}

void dump_available_options()
{
	for (auto&& line : conf::format_options())
		u8cout << line << "\n";
}


fs::path make_temp_file()
{
	static fs::path dir = paths::temp_dir();

	wchar_t name[MAX_PATH + 1] = {};
	if (GetTempFileNameW(dir.native().c_str(), L"mob", 0, name) == 0)
	{
		const auto e = GetLastError();

		gcx().bail_out(context::conf,
			"can't create temp file in {}, {}", dir, error_message(e));
	}

	return dir / name;
}

}	// namespace
