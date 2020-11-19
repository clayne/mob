#include "pch.h"
#include "tools.h"
#include "../core/process.h"

namespace mob
{

cmake::cmake(ops o)
	: basic_process_runner("cmake"), op_(o), gen_(jom), arch_(arch::def)
{
}

fs::path cmake::binary()
{
	return conf::tool_by_name("cmake");
}

cmake& cmake::generator(generators g)
{
	gen_ = g;
	return *this;
}

cmake& cmake::generator(const std::string& g)
{
	genstring_ = g;
	return *this;
}

cmake& cmake::root(const fs::path& p)
{
	root_ = p;
	return *this;
}

cmake& cmake::output(const fs::path& p)
{
	output_ = p;
	return *this;
}

cmake& cmake::prefix(const fs::path& s)
{
	prefix_ = s;
	return *this;
}

cmake& cmake::def(const std::string& name, const std::string& value)
{
	def_.emplace_back(name, value);
	return *this;
}

cmake& cmake::def(const std::string& name, const fs::path& p)
{
	def(name, path_to_utf8(p));
	return *this;
}

cmake& cmake::def(const std::string& name, const char* s)
{
	def(name, std::string(s));
	return *this;
}

cmake& cmake::architecture(arch a)
{
	arch_ = a;
	return *this;
}

cmake& cmake::cmd(const std::string& s)
{
	cmd_ = s;
	return *this;
}

fs::path cmake::build_path() const
{
	if (!output_.empty())
		return output_;

	const auto& g = get_generator(gen_);
	return root_ / (g.output_dir(arch_));
}

fs::path cmake::result() const
{
	return build_path();
}

void cmake::do_run()
{
	switch (op_)
	{
		case clean:
		{
			do_clean();
			break;
		}

		case generate:
		{
			do_generate();
			break;
		}

		default:
		{
			cx().bail_out(context::generic, "bad cmake op {}", op_);
		}
	}
}


void cmake::do_generate()
{
	if (root_.empty())
		cx().bail_out(context::generic, "cmake output path is empty");

	const auto& g = get_generator(gen_);

	auto p = process()
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.binary(binary())
		.arg("-DCMAKE_BUILD_TYPE=Release")
		.arg("-DCMAKE_INSTALL_MESSAGE=NEVER")
		.arg("--log-level=ERROR")
		.arg("--no-warn-unused-cli");

	if (genstring_.empty())
	{
		p
			.arg("-G", "\"" + g.name + "\"")
			.arg(g.get_arch(arch_));
	}
	else
	{
		p
			.arg("-G", "\"" + genstring_ + "\"");
	}

	if (!prefix_.empty())
		p.arg("-DCMAKE_INSTALL_PREFIX=", prefix_);

	for (auto&& [name, value] : def_)
		p.arg("-D" + name + "=" + value + "");

	if (cmd_.empty())
		p.arg("..");
	else
		p.arg(cmd_);

	p
		.env(env::vs(arch_)
			.set("CXXFLAGS", "/wd4566"))
		.cwd(build_path());

	set_process(p);
	execute_and_join();
}

void cmake::do_clean()
{
	cx().trace(context::rebuild, "deleting all generator directories");
	op::delete_directory(cx(), build_path(), op::optional);
}

const std::map<cmake::generators, cmake::gen_info>& cmake::all_generators()
{
	static const std::map<generators, gen_info> map =
	{
		{ generators::jom, {"build", "NMake Makefiles JOM", "", "" }},

		{ generators::vs, {
			"vsbuild",
			"Visual Studio " + vs::version() + " " + vs::year(),
			"Win32",
			"x64"
		}}
	};

	return map;
}

const cmake::gen_info& cmake::get_generator(generators g)
{
	const auto& map = all_generators();

	auto itor = map.find(g);
	if (itor == map.end())
		bail_out("unknown generator");

	return itor->second;
}

std::string cmake::gen_info::get_arch(arch a) const
{
	switch (a)
	{
		case arch::x86:
		{
			if (x86.empty())
				return {};
			else
				return "-A " + x86;
		}

		case arch::x64:
		{
			if (x64.empty())
				return {};
			else
				return "-A " + x64;
		}

		case arch::dont_care:
			return {};

		default:
			bail_out("gen_info::get_arch(): bad arch");
	}
}

std::string cmake::gen_info::output_dir(arch a) const
{
	switch (a)
	{
		case arch::x86:
			return (dir + "_32");

		case arch::x64:
		case arch::dont_care:
			return dir;

		default:
			bail_out("gen_info::get_arch(): bad arch");
	}
}

}	// namespace
