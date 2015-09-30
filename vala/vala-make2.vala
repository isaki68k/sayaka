//
// vala のソースの依存関係を確認してビルドしたりするツール
// Copyright (C) 2015 isaki@NetBSD.org
// vala-make2.vala Copyright (C) 2015 @moveccr
//

public class Program
{
	public static int main(string[] args)
	{
		return new Program().main2(args);
	}

	public void usage()
	{
		SetDefaultOpt();

		var msg =
@"
 使い方
	vala-make -o <exefile> [<options...>] <srcs...>

	<srcs...> (.valaファイル) から実行ファイル <exefile> をビルドします。
	その際各ファイル同士のタイムスタンプを比較し、必要なコマンドだけを
	実行するように努めます。

	  -a <valac_cmd>
		vala コンパイラのコマンド(とオプション)を指定します。
		デフォルトは $(Q(opt_vala_cmd)) です。
		互換性: オプションのうち -C は取り除かれます。

	  -c <cc_cmd>
		C コンパイラのコマンド(とオプション)を指定します。
		デフォルトは $(Q(opt_cc_cmd)) です。

	  -d
		デバッグ表示をします。

	  -e
		エコーモードです。コマンドを実行する際にそのコマンドラインを
		表示します。make で @./vala-make と組み合わせるとそれっぽく
		なるかも。

	  -l <ld_cmd>
		C リンカのコマンド(とオプション)を指定します。
		デフォルトは $(Q(opt_ld_cmd)) です。

	  -L <libs>
		リンカに指定するライブラリ等を指定します。

	  -n
		dry-run です。実際には何も実行しません。
		実行しなかったことによりタイムスタンプが更新されず、その後の
		実行結果の表示が -n なしの時とは異なる場合がありますが、限界です。

	  -o <exefile>
		実行ファイル名です。
		デフォルトは $(Q(opt_exefile)) です。

	  -O <objdir>
		中間ファイルである .c と .o を置くディレクトリを指定します。
		デフォルトは $(Q(opt_workdir)) です。
";


		stdout.printf("%s\n", msg);
	}

	// usage 中のクォート用
	public string Q(string s)
	{
		return "\"" + s + "\"";
	}

	private string opt_vala_cmd;
	private string opt_vala_opt;
	private string opt_cc_cmd;
	private string opt_ld_cmd;
	private string opt_exefile;
	private string opt_libs;
	private string opt_workdir;
	private bool opt_debug;
	private bool opt_dry_run;
	private bool opt_echocmd;

	private Array<string> srcfiles;
	// srcfiles のうち .vala なファイル名。
	private Array<string> src_vala;
	// srcfiles のうち .c なファイル名。
	private Array<string> src_c;

	// オプションのデフォルト値を(再)設定します。
	public void SetDefaultOpt()
	{
		opt_vala_cmd = "valac";
		opt_vala_opt = "";
		opt_cc_cmd = "cc";
		opt_ld_cmd = "cc";
		opt_exefile = "a.out";
		opt_libs = "";
		opt_workdir = ".";
		opt_debug = false;
		opt_dry_run = false;
		opt_echocmd = false;
	}

	public int main2(string[] args)
	{
		SetDefaultOpt();

		srcfiles = new Array<string>();
		src_vala = new Array<string>();
		src_c = new Array<string>();

		for (int i = 1; i < args.length; i++) {
			switch (args[i]) {
			 case "-a":
				opt_vala_cmd = args[++i];
				break;
			 case "-c":
				opt_cc_cmd = args[++i];
				break;
			 case "-d":
				opt_debug = true;
				break;
			 case "-e":
				opt_echocmd = true;
				break;
			 case "-l":
				opt_ld_cmd = args[++i];
				break;
			 case "-L":
				opt_libs = args[++i];
				break;
			 case "-n":
				opt_dry_run = true;
				break;
			 case "-o":
				opt_exefile = args[++i];
				break;

			 case "--valaopt":
				opt_vala_opt = args[++i];
				break;

			 case "-O":
			 case "--workdir":
				opt_workdir = args[++i];
				break;

			 case "--cfile":
				src_c.append_val(args[++i]);
				// srcfiles にも足しておく。
				srcfiles.append_val(args[i]);
				break;

			 default:
				if (args[i][0] == '-') {
					usage();
					return 1;
				}
				src_vala.append_val(args[i]);
				// srcfiles にも足しておく。
				srcfiles.append_val(args[i]);
				break;
			}
		}

		if (opt_vala_cmd == "" || srcfiles.length == 0) {
			usage();
			return 1;
		}

		if (opt_debug) {
			stdout.puts("srcfiles:");
			for (int i = 0; i < srcfiles.length; i++) {
				stdout.puts(@" $(srcfiles.data[i])");
			}
			stdout.puts("\n");
		}

		// ディレクトリセパレータ正規化
		if (opt_workdir == "") {
			opt_workdir = "./";
		} else if (opt_workdir.has_suffix("/") == false) {
			opt_workdir += "/";
		}

		Posix.mkdir(opt_workdir, 0777);

		// .vala -> .vapi

		for (int i = 0; i < src_vala.length; i++) {
			var valafile = src_vala.data[i];

			if (FileUtils.test(valafile, FileTest.EXISTS) == false) {
				stdout.puts(@"$(valafile) not exists.\n");
				return 1;
			}

			var vapifile = opt_workdir + ChangeExt(valafile, ".vapi");

			if (need_update(valafile, vapifile)) {
				var cmd = @"$(opt_vala_cmd) --fast-vapi=$(vapifile) $(valafile)";
				run_cmd(cmd);
				// vala は変更がないとき日付を更新しないので touch
				FileUtils.utime(vapifile);
			}
		}

		// .vala .vapi -> .c
		for (int i = 0; i < src_vala.length; i++) {
			var valafile = src_vala.data[i];
			var cfile = ChangeExt(valafile, ".c");

			if (need_update(valafile, cfile)) {
				var cmd = @"$(opt_vala_cmd) $(opt_vala_opt) -C";
				foreach (var f in src_vala.data) {
					if (f == valafile) continue;

					var vapifile = opt_workdir + ChangeExt(f, ".vapi");
					cmd += @" --use-fast-vapi=$(vapifile)";
				}
				cmd += @" $(valafile)";

				run_cmd(cmd);
				// vala は変更がないとき日付を更新しないので touch
				FileUtils.utime(cfile);
			}
		}

		// .c -> .o
		for (int i = 0; i < srcfiles.length; i++) {
			var cfile = ChangeExt(srcfiles.data[i], ".c");
			var ofile = ChangeExt(cfile, ".o");

			if (need_update(cfile, ofile)) {
				var cmd = @"$(opt_cc_cmd) -c $(cfile) -o $(ofile)";
				run_cmd(cmd);
			}
		}

		// .o -> target_exe
		var need_exe_update = false;
		for (int i = 0; i < srcfiles.length; i++) {
			var ofile = ChangeExt(srcfiles.data[i], ".o");
			need_exe_update = need_update(ofile, opt_exefile);
			if (need_exe_update) break;
		}
		if (need_exe_update) {
			var cmd = @"$(opt_ld_cmd) -o $(opt_exefile)";
			for (int i = 0; i < srcfiles.length; i++) {
				var ofile = ChangeExt(srcfiles.data[i], ".o");
				cmd += " " + ofile;
			}
			cmd += " " + opt_libs;
			run_cmd(cmd);
		}

		return 0;
	}

	private int64 GetFileTime(string file)
	{
		var st = (Posix.Stat)Stat(file);

		return st.st_mtime;
	}

	private bool need_update(string srcfile, string dstfile)
	{
		bool rv = false;

		if (opt_debug) {
			stdout.puts(@"checking $(srcfile) and $(dstfile) .. ");
			stdout.flush();
		}

		var t1 = GetFileTime(srcfile);
		var t2 = GetFileTime(dstfile);

		if (t1 > t2) {
			rv = true;
		}

		if (opt_debug) {
			if (rv) {
				stdout.puts("need-to-update\n");
			} else {
				stdout.puts("no-update\n");
			}
		}
		return rv;
	}

	private void run_cmd(string cmd)
	{
		if (opt_dry_run) {
			stdout.puts(cmd + "\n");
		} else {
			if (opt_echocmd) {
				stdout.puts(cmd + "\n");
			}
			bool r = false;
			int exit_status = 0;
			try {
				r = Process.spawn_command_line_sync(
					cmd, null, null, out exit_status);
				if (exit_status != 0) {
					r = false;
				}
			} catch (Error e) {
				stderr.puts(e.message);
				r = false;
			}

			if (r == false) {
				Process.exit(1);
			}
		}
	}

	// s の拡張子を ext に変更します。ext にピリオドが無いと拡張子が
	// つながって無くなってしまいます。
	public static string ChangeExt(string s, string ext)
	{
		int i = s.last_index_of(".");
		if (i == -1) {
			return s + ext;
		}
		return s.substring(0, i) + ext;
	}
}

