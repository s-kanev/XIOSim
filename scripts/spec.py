#! /usr/bin/python
import os.path

#Configuration params
specDir = '/home/skanev/cpu2006/benchspec/CPU2006'
specExt = 'O3gcc4static241'
specInput = 'ref'
specTune = 'base'
specRunID = '0002'

def GetDir(bmk):
    return os.path.join(specDir, bmk, 'run', 'run_%s_%s_%s.%s' % (specTune, specInput, specExt, specRunID))

class BenchmarkRun(object):

    def __init__(self, bmk, executable, args, input, output, error, name):
        self.bmk = bmk
        self.executable = '%s_%s.%s' % (executable, specTune, specExt)
        self.args = args
        self.input = input
        self.output = output
        self.error = error
        self.directory = GetDir(bmk)
        self.name = "%s.%s" % (self.bmk, name)

runs = (
        BenchmarkRun('400.perlbench', 'perlbench', '-I./lib checkspam.pl 2500 5 25 11 150 1 1 1 1', '', 'checkspam.2500.5.25.11.150.1.1.1.1.out', 'checkspam.2500.5.25.11.150.1.1.1.1.err', 'checkspam'),
        BenchmarkRun('400.perlbench', 'perlbench', '-I./lib diffmail.pl 4 800 10 17 19 300', '', 'diffmail.4.800.10.17.19.300.out', 'diffmail.4.800.10.17.19.300.err', 'diffmail'),
        BenchmarkRun('400.perlbench', 'perlbench', '-I./lib splitmail.pl 1600 12 26 16 4500', '', 'splitmail.1600.12.26.16.4500.out', 'splitmail.1600.12.26.16.4500.err', 'splitmail'),

        BenchmarkRun('401.bzip2', 'bzip2', 'input.source 280', '', 'input.source.out', 'input.source.err', 'source'),
        BenchmarkRun('401.bzip2', 'bzip2', 'chicken.jpg 30', '' ,'chicken.jpg.out', 'chicken.jpg.err', 'chicken'),
        BenchmarkRun('401.bzip2', 'bzip2', 'liberty.jpg 30', '', 'liberty.jpg.out', 'liberty.jpg.err', 'liberty'),
        BenchmarkRun('401.bzip2', 'bzip2', 'input.program 280', '', 'input.program.out', 'input.program.err', 'program'),
        BenchmarkRun('401.bzip2', 'bzip2', 'text.html 280', '', 'text.html.out', 'text.html.err', 'text'),
        BenchmarkRun('401.bzip2', 'bzip2', 'input.combined 200', '', 'input.combined.out', 'input.combined.err', 'combined'),

        BenchmarkRun('403.gcc', 'gcc', '166.i -o 166.s', '', '166.out', '166.err', '166'),
        BenchmarkRun('403.gcc', 'gcc', '200.i -o 200.s', '', '200.out', '200.err', '200'),
        BenchmarkRun('403.gcc', 'gcc', 'c-typeck.i -o c-typeck.s', '', 'c-typeck.out', 'c-typeck.err', 'c-typecheck'),
        BenchmarkRun('403.gcc', 'gcc', 'cp-decl.i -o cp-decl.s', '', 'cp-decl.out', 'cp-decl.err', 'cp-decl'),
        BenchmarkRun('403.gcc', 'gcc', 'expr.i -o expr.s', '', 'expr.out', 'expr.err', 'expr'),
        BenchmarkRun('403.gcc', 'gcc', 'expr2.i -o expr2.s', '', 'expr2.out', 'expr2.err', 'expr2'),
        BenchmarkRun('403.gcc', 'gcc', 'g23.i -o g23.s', '', 'g23.out', 'g23.err', 'g23'),
        BenchmarkRun('403.gcc', 'gcc', 's04.i -o s04.s', '', 's04.out', 's04.err', 's04'),
        BenchmarkRun('403.gcc', 'gcc', 'scilab.i -o scilab.s', '', 'scilab.out', 'scilab.err', 'scilab'),

        BenchmarkRun('429.mcf', 'mcf', 'inp.in', '', 'inp.out', 'inp.err', 'inp'),

        BenchmarkRun('445.gobmk', 'gobmk', '--quiet --mode gtp', '13x13.tst', '13x13.out', '13x13.err', '13x13'),
        BenchmarkRun('445.gobmk', 'gobmk', '--quiet --mode gtp', 'nngs.tst', 'nngs.out', 'nngs.err', 'nngs'),
        BenchmarkRun('445.gobmk', 'gobmk', '--quiet --mode gtp', 'score2.tst', 'score2.out', 'score2.err', 'score2'),
        BenchmarkRun('445.gobmk', 'gobmk', '--quiet --mode gtp', 'trevorc.tst', 'trevorc.out', 'trevorc.err', 'trevorc'),
        BenchmarkRun('445.gobmk', 'gobmk', '--quiet --mode gtp', 'trevord.tst', 'trevord.out', 'trevord.err', 'trevord'),

        BenchmarkRun('456.hmmer', 'hmmer', 'nph3.hmm swiss41', '', 'nph3.out' ,'nph3.err', 'nph3'),
        BenchmarkRun('456.hmmer', 'hmmer', '--fixed 0 --mean 500 --num 500000 --sd 350 --seed 0 retro.hmm', '', 'retro.out', 'retro.err', 'retro'),

        BenchmarkRun('458.sjeng', 'sjeng', 'ref.txt', '', 'ref.out', 'ref.err', 'ref'),

        BenchmarkRun('462.libquantum', 'libquantum', 'libquantum 1397 8', '', 'ref.out', 'ref.err', 'ref'),

        BenchmarkRun('464.h264ref', 'h264ref', '-d foreman_ref_encoder_baseline.cfg', '', 'foreman_ref_baseline_encodelog.out', 'foreman_ref_baseline_encodelog.err', 'foreman_ref_baseline_encodelog'),
        BenchmarkRun('464.h264ref', 'h264ref', '-d foreman_ref_encoder_main.cfg', '', 'foreman_ref_main_encodelog.out', 'foreman_ref_main_encodelog.err', 'foreman_ref_main_encodelog'),
        BenchmarkRun('464.h264ref', 'h264ref', '-d sss_encoder_main.cfg', '', 'sss_main_encodelog.out', 'sss_main_encodelog.err', 'sss_main_encodelog'),

        BenchmarkRun('471.omnetpp', 'omnetpp', 'omnetpp.ini', '', 'omnetpp.log' ,'omnetpp.err', 'omnetpp'),

        BenchmarkRun('473.astar', 'astar', 'BigLakes2048.cfg', '', 'BigLakes2048.out', 'BigLakes2048.err', 'BigLakes2048'),
        BenchmarkRun('473.astar', 'astar', 'rivers.cfg', '', 'rivers.out', 'rivers.err', 'rivers'),

        BenchmarkRun('483.xalancbmk', 'Xalan', '-v t5.xml xalanc.xsl', '', 'ref.out', 'ref.err', 'ref'),

        BenchmarkRun('998.specrand', 'specrand', '1255432124 234923', '', 'rand.234923.out', 'rand.234923.err', 'rand.234923'),

        BenchmarkRun('410.bwaves', 'bwaves', '', '', 'bwaves.out', 'bwaves.err', 'bwaves'),

        BenchmarkRun('416.gamess', 'gamess', '', 'cytosine.2.config', 'cytosine.2.out', 'cytosine.2.err', 'cytosine'),
        BenchmarkRun('416.gamess', 'gamess', '', 'h2ocu2+.gradient.config', 'h2ocu2+.gradient.out', 'h2ocu2+.gradient.err', 'h2ocu2+.gradient'),
        BenchmarkRun('416.gamess', 'gamess', '', 'triazolium.config', 'triazolium.out', 'triazolium.err', 'triazolium'),

        BenchmarkRun('433.milc', 'milc', '', 'su3imp.in', 'su3imp.out', 'su3imp.err', 'su3imp'),

        BenchmarkRun('434.zeusmp', 'zeusmp', '', '', 'zeusmp.stdout', 'zeusmp.err', 'zeusmp'),

        BenchmarkRun('435.gromacs', 'gromacs', '-silent -deffnm gromacs -nice 0', '', 'gromacs.out', 'gromacs.errQ', 'gromacs'),

        BenchmarkRun('436.cactusADM', 'cactusADM', 'benchADM.par', '', 'benchADM.out', 'benchADM.err', 'benchADM'),

        BenchmarkRun('437.leslie3d', 'leslie3d', '', 'leslie3d.in', 'leslie3d.stdout', 'leslie3d.err', 'leslie3d'),

        BenchmarkRun('444.namd', 'namd', '--input namd.input --iterations 38 --output namd.out', '', 'namd.stdout', 'namd.err', 'namd'),

        BenchmarkRun('447.dealII', 'dealII', '23', '', 'log', 'dealII.err', 'dealII'),

        BenchmarkRun('450.soplex', 'soplex', '-s1 -e -m45000 pds-50.mps', '', 'pds-50.mps.out', 'pds-50.mps.stderr', 'pds-50'),
        BenchmarkRun('450.soplex', 'soplex', '-m3500 ref.mps', '', 'ref.out', 'ref.stderr', 'ref'),

        BenchmarkRun('453.povray', 'povray', 'SPEC-benchmark-ref.ini', '', 'SPEC-benchmark-ref.stdout', 'SPEC-benchmark-ref.stderr', 'SPEC-benchmark-ref'),

        BenchmarkRun('454.calculix', 'calculix', '-i  hyperviscoplastic', '', 'hyperviscoplastic.log', 'hyperviscoplastic.err', 'hyperviscoplastic'),

        BenchmarkRun('459.GemsFDTD', 'GemsFDTD', '', '', 'ref.log', 'ref.err', 'ref'),

        BenchmarkRun('465.tonto', 'tonto', '', '', 'tonto.out', 'tonto.err', 'tonto'),

        BenchmarkRun('470.lbm', 'lbm', '3000 reference.dat 0 0 100_100_130_ldc.of', '', 'lbm.out', 'lbm.err', 'lbm'),

        BenchmarkRun('481.wrf', 'wrf', 'rsl.out.0000', '', 'wrf.out', 'wrf.err', 'wrf'),

        BenchmarkRun('482.sphinx3', 'sphinx_livepretend', 'ctlfile . args.an4', '', 'an4.log', 'an4.err', 'an4'),

        BenchmarkRun('999.specrand', 'specrand', '1255432124 234923', '', 'rand.234923.out', 'rand.234923.err', 'rand.234923')
       )
