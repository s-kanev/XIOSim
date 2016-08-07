#!/usr/bin/python
import os.path

import bmk

# Configuration params
specDir = '/group/brooks/skanev/cpu2006/benchspec/CPU2006'
specExt = 'O3gcc4static241'
specInput = 'ref'
specTune = 'base'
specRunID = '0000'


class SPECRun(bmk.BenchmarkRun):

    def __init__(self, bmk, executable, args, input, output, error, name, expected):
        binary = '%s_%s.%s' % (executable, specTune, specExt)
        super(SPECRun, self).__init__(bmk, binary, args, input, output, error, name, expected)
        self.needs_pin_points = True
        self.needs_roi = False

    def GetDir(self):
        return os.path.join(specDir, self.bmk, 'run', 'run_%s_%s_%s.%s' % (specTune, specInput, specExt, specRunID))


runs = [
        SPECRun('400.perlbench', 'perlbench', '-I./lib checkspam.pl 2500 5 25 11 150 1 1 1 1', '', 'checkspam.2500.5.25.11.150.1.1.1.1.out', 'checkspam.2500.5.25.11.150.1.1.1.1.err', 'checkspam', ['lib', 'rules', 'cpu2006_mhonarc.rc', 'checkspam.pl', 'checkspam.in']),
        SPECRun('400.perlbench', 'perlbench', '-I./lib diffmail.pl 4 800 10 17 19 300', '', 'diffmail.4.800.10.17.19.300.out', 'diffmail.4.800.10.17.19.300.err', 'diffmail', ['lib', 'rules', 'cpu2006_mhonarc.rc', 'diffmail.pl', 'diffmail.in']),
        SPECRun('400.perlbench', 'perlbench', '-I./lib splitmail.pl 1600 12 26 16 4500', '', 'splitmail.1600.12.26.16.4500.out', 'splitmail.1600.12.26.16.4500.err', 'splitmail', ['lib', 'rules', 'cpu2006_mhonarc.rc', 'splitmail.pl', 'splitmail.in']),

        SPECRun('401.bzip2', 'bzip2', 'input.source 280', '', 'input.source.out', 'input.source.err', 'source', ['input.source']),
        SPECRun('401.bzip2', 'bzip2', 'chicken.jpg 30', '' ,'chicken.jpg.out', 'chicken.jpg.err', 'chicken', ['chicken.jpg']),
        SPECRun('401.bzip2', 'bzip2', 'liberty.jpg 30', '', 'liberty.jpg.out', 'liberty.jpg.err', 'liberty', ['liberty.jpg']),
        SPECRun('401.bzip2', 'bzip2', 'input.program 280', '', 'input.program.out', 'input.program.err', 'program', ['input.program']),
        SPECRun('401.bzip2', 'bzip2', 'text.html 280', '', 'text.html.out', 'text.html.err', 'text', ['text.html']),
        SPECRun('401.bzip2', 'bzip2', 'input.combined 200', '', 'input.combined.out', 'input.combined.err', 'combined', ['input.combined']),

        SPECRun('403.gcc', 'gcc', '166.i -o 166.s', '', '166.out', '166.err', '166', ['166.i']),
        SPECRun('403.gcc', 'gcc', '200.i -o 200.s', '', '200.out', '200.err', '200', ['200.i']),
        SPECRun('403.gcc', 'gcc', 'c-typeck.i -o c-typeck.s', '', 'c-typeck.out', 'c-typeck.err', 'c-typeck', ['c-typeck.i']),
        SPECRun('403.gcc', 'gcc', 'cp-decl.i -o cp-decl.s', '', 'cp-decl.out', 'cp-decl.err', 'cp-decl', ['cp-decl.i']),
        SPECRun('403.gcc', 'gcc', 'expr.i -o expr.s', '', 'expr.out', 'expr.err', 'expr', ['expr.i']),
        SPECRun('403.gcc', 'gcc', 'expr2.i -o expr2.s', '', 'expr2.out', 'expr2.err', 'expr2', ['expr2.i']),
        SPECRun('403.gcc', 'gcc', 'g23.i -o g23.s', '', 'g23.out', 'g23.err', 'g23', ['g23.i']),
        SPECRun('403.gcc', 'gcc', 's04.i -o s04.s', '', 's04.out', 's04.err', 's04', ['s04.i']),
        SPECRun('403.gcc', 'gcc', 'scilab.i -o scilab.s', '', 'scilab.out', 'scilab.err', 'scilab', ['scilab.i']),

        SPECRun('429.mcf', 'mcf', 'inp.in', '', 'inp.out', 'inp.err', 'inp', ['inp.in']),

        SPECRun('445.gobmk', 'gobmk', '--quiet --mode gtp', '13x13.tst', '13x13.out', '13x13.err', '13x13', ['games', 'golois']),
        SPECRun('445.gobmk', 'gobmk', '--quiet --mode gtp', 'nngs.tst', 'nngs.out', 'nngs.err', 'nngs', ['games', 'golois']),
        SPECRun('445.gobmk', 'gobmk', '--quiet --mode gtp', 'score2.tst', 'score2.out', 'score2.err', 'score2', ['games', 'golois']),
        SPECRun('445.gobmk', 'gobmk', '--quiet --mode gtp', 'trevorc.tst', 'trevorc.out', 'trevorc.err', 'trevorc', ['games', 'golois']),
        SPECRun('445.gobmk', 'gobmk', '--quiet --mode gtp', 'trevord.tst', 'trevord.out', 'trevord.err', 'trevord', ['games', 'golois']),

        SPECRun('456.hmmer', 'hmmer', 'nph3.hmm swiss41', '', 'nph3.out' ,'nph3.err', 'nph3', ['swiss41', 'nph3.hmm']),
        SPECRun('456.hmmer', 'hmmer', '--fixed 0 --mean 500 --num 500000 --sd 350 --seed 0 retro.hmm', '', 'retro.out', 'retro.err', 'retro', ['retro.hmm']),

        SPECRun('458.sjeng', 'sjeng', 'ref.txt', '', 'ref.out', 'ref.err', 'ref', ['ref.txt']),

        SPECRun('462.libquantum', 'libquantum', '1397 8', '', 'ref.out', 'ref.err', 'ref', ['control']),

        SPECRun('464.h264ref', 'h264ref', '-d foreman_ref_encoder_baseline.cfg', '', 'foreman_ref_baseline_encodelog.out', 'foreman_ref_baseline_encodelog.err', 'foreman_ref_baseline_encodelog', ['foreman_qcif.yuv', 'leakybucketrate.cfg', 'foreman_ref_encoder_baseline.cfg']),
        SPECRun('464.h264ref', 'h264ref', '-d foreman_ref_encoder_main.cfg', '', 'foreman_ref_main_encodelog.out', 'foreman_ref_main_encodelog.err', 'foreman_ref_main_encodelog', ['foreman_qcif.yuv', 'leakybucketrate.cfg', 'foreman_ref_encoder_main.cfg']),
        SPECRun('464.h264ref', 'h264ref', '-d sss_encoder_main.cfg', '', 'sss_main_encodelog.out', 'sss_main_encodelog.err', 'sss_main_encodelog', ['foreman_qcif.yuv', 'leakybucketrate.cfg', 'sss.yuv', 'sss_encoder_main.cfg']),

        SPECRun('471.omnetpp', 'omnetpp', 'omnetpp.ini', '', 'omnetpp.log' ,'omnetpp.err', 'omnetpp', ['omnetpp.ini']),

        SPECRun('473.astar', 'astar', 'BigLakes2048.cfg', '', 'BigLakes2048.out', 'BigLakes2048.err', 'BigLakes2048', ['BigLakes2048.bin', 'BigLakes2048.cfg']),
        SPECRun('473.astar', 'astar', 'rivers.cfg', '', 'rivers.out', 'rivers.err', 'rivers', ['rivers.bin', 'rivers.cfg']),

        SPECRun('483.xalancbmk', 'Xalan', '-v t5.xml xalanc.xsl', '', 'ref.out', 'ref.err', 'ref', ['xalanc.xsl', 'ref.lst', '100mb.xsd', 't5.xml']),

#        SPECRun('998.specrand', 'specrand', '1255432124 234923', '', 'rand.234923.out', 'rand.234923.err', 'rand.234923'),

        SPECRun('410.bwaves', 'bwaves', '', '', 'bwaves.out', 'bwaves.err', 'bwaves', ['bwaves.in']),

        SPECRun('416.gamess', 'gamess', '', 'cytosine.2.config', 'cytosine.2.out', 'cytosine.2.err', 'cytosine', ['cytosine.2.inp']),
        SPECRun('416.gamess', 'gamess', '', 'h2ocu2+.gradient.config', 'h2ocu2+.gradient.out', 'h2ocu2+.gradient.err', 'h2ocu2+', ['h2ocu2+.gradient.inp']),
        SPECRun('416.gamess', 'gamess', '', 'triazolium.config', 'triazolium.out', 'triazolium.err', 'triazolium', ['triazolium.inp']),

        SPECRun('433.milc', 'milc', '', 'su3imp.in', 'su3imp.out', 'su3imp.err', 'su3imp', []),

        SPECRun('434.zeusmp', 'zeusmp', '', '', 'zeusmp.stdout', 'zeusmp.err', 'zeusmp', ['zmp_inp']),

        SPECRun('435.gromacs', 'gromacs', '-silent -deffnm gromacs -nice 0', '', 'gromacs.out', 'gromacs.errQ', 'gromacs', ['gromacs.tpr']),

        SPECRun('436.cactusADM', 'cactusADM', 'benchADM.par', '', 'benchADM.out', 'benchADM.err', 'benchADM', ['benchADM.par']),

        SPECRun('437.leslie3d', 'leslie3d', '', 'leslie3d.in', 'leslie3d.stdout', 'leslie3d.err', 'leslie3d', []),

        SPECRun('444.namd', 'namd', '--input namd.input --iterations 38 --output namd.out', '', 'namd.stdout', 'namd.err', 'namd', ['namd.input']),

        SPECRun('447.dealII', 'dealII', '23', '', 'log', 'dealII.err', 'dealII', ['DummyData']),

        SPECRun('450.soplex', 'soplex', '-s1 -e -m45000 pds-50.mps', '', 'pds-50.mps.out', 'pds-50.mps.stderr', 'pds-50', ['pds-50.mps']),
        SPECRun('450.soplex', 'soplex', '-m3500 ref.mps', '', 'ref.out', 'ref.stderr', 'ref', ['ref.mps']),

        SPECRun('453.povray', 'povray', 'SPEC-benchmark-ref.ini', '', 'SPEC-benchmark-ref.stdout', 'SPEC-benchmark-ref.stderr', 'SPEC-benchmark-ref', ['shapes_old.inc', 'povlogo.ttf', 'cyrvetic.ttf', 'skies.inc', 'finish.inc', 'glass_old.inc', 'shapes.inc', 'stdinc.inc', 'stage1.inc', 'rand.inc', 'rad_def.inc', 'metals.inc', 'sunpos.inc', 'timrom.ttf', 'stones1.inc', 'stars.inc', 'stones2.inc', 'colors.inc', 'functions.inc', 'golds.inc', 'consts.inc', 'glass.inc', 'stones.inc', 'stdcam.inc', 'screen.inc', 'textures.inc', 'debug.inc', 'stoneold.inc', 'math.inc', 'strings.inc', 'woodmaps.inc', 'woods.inc', 'transforms.inc', 'chars.inc', 'logo.inc', 'arrays.inc', 'shapesq.inc', 'shapes2.inc', 'crystal.ttf', 'SPEC-benchmark-ref.ini', 'SPEC-benchmark-ref.pov']),

        SPECRun('454.calculix', 'calculix', '-i  hyperviscoplastic', '', 'hyperviscoplastic.log', 'hyperviscoplastic.err', 'hyperviscoplastic', ['hyperviscoplastic.inp']),

        SPECRun('459.GemsFDTD', 'GemsFDTD', '', '', 'ref.log', 'ref.err', 'ref', ['ref.in', 'sphere.pec', 'yee.dat']),

        SPECRun('465.tonto', 'tonto', '', 'stdin', 'tonto.out', 'tonto.err', 'tonto', []),

        SPECRun('470.lbm', 'lbm', '3000 reference.dat 0 0 100_100_130_ldc.of', '', 'lbm.out', 'lbm.err', 'lbm', ['lbm.in', '100_100_130_ldc.of']),

        SPECRun('481.wrf', 'wrf', 'rsl.out.0000', '', 'wrf.out', 'wrf.err', 'wrf', ['be', 'le', 'wrf.in', 'GENPARM.TBL', 'LANDUSE.TBL', 'SOILPARM.TBL', 'VEGPARM.TBL', 'namelist.input', 'wrfbdy_d01', 'wrfinput_d01']),

        SPECRun('482.sphinx3', 'sphinx_livepretend', 'ctlfile . args.an4', '', 'an4.log', 'an4.err', 'an4', ['model', 'ctlfile', 'an392-mjwl-b.raw', 'cen8-mmxg-b.raw', 'cen1-miry-b.raw', 'an402-mdms2-b.raw', 'cen2-mdms2-b.raw', 'cen5-miry-b.raw', 'cen1-mjgm-b.raw', 'an434-marh-b.raw', 'an396-miry-b.raw', 'cen4-marh-b.raw', 'an424-menk-b.raw', 'cen5-mjgm-b.raw', 'cen8-marh-b.raw', 'an439-mjgm-b.raw', 'an429-fvap-b.raw', 'beams.dat', 'an443-mmxg-b.raw', 'an401-mdms2-b.raw', 'cen1-mdms2-b.raw', 'an440-mjgm-b.raw', 'cen1-fjlp-b.raw', 'an430-fvap-b.raw', 'cen3-menk-b.raw', 'an416-fjlp-b.raw', 'cen4-fvap-b.raw', 'cen5-fjlp-b.raw', 'cen7-menk-b.raw', 'cen8-fvap-b.raw', 'cen3-mmxg-b.raw', 'cen4-mjwl-b.raw', 'an391-mjwl-b.raw', 'cen7-mmxg-b.raw', 'cen8-mjwl-b.raw', 'an395-mjwl-b.raw', 'cen4-miry-b.raw', 'cen8-miry-b.raw', 'an433-marh-b.raw', 'cen3-marh-b.raw', 'cen8-mdms2-b.raw', 'an423-menk-b.raw', 'an399-miry-b.raw', 'args.an4', 'cen4-mjgm-b.raw', 'cen7-marh-b.raw', 'an438-mjgm-b.raw', 'cen8-mjgm-b.raw', 'an428-fvap-b.raw', 'an442-mmxg-b.raw', 'cen7-mdms2-b.raw', 'cen2-menk-b.raw', 'cen3-fvap-b.raw', 'cen4-fjlp-b.raw', 'cen6-menk-b.raw', 'an419-fjlp-b.raw', 'cen7-miry-b.raw', 'cen6-mdms2-b.raw', 'cen2-marh-b.raw', 'an432-marh-b.raw', 'an420-fjlp-b.raw', 'an422-menk-b.raw', 'an398-miry-b.raw', 'cen3-mjgm-b.raw', 'cen6-marh-b.raw', 'an437-mjgm-b.raw', 'cen7-mjgm-b.raw', 'an427-fvap-b.raw', 'an441-mmxg-b.raw', 'an400-miry-b.raw', 'an445-mmxg-b.raw', 'an405-mdms2-b.raw', 'cen5-mdms2-b.raw', 'cen1-menk-b.raw', 'cen2-fvap-b.raw', 'cen3-fjlp-b.raw', 'cen5-menk-b.raw', 'an418-fjlp-b.raw', 'cen6-fvap-b.raw', 'cen7-fjlp-b.raw', 'cen1-mmxg-b.raw', 'cen2-mjwl-b.raw', 'cen5-mmxg-b.raw', 'cen6-mjwl-b.raw', 'an393-mjwl-b.raw', 'cen2-miry-b.raw', 'an404-mdms2-b.raw', 'cen4-mdms2-b.raw', 'cen6-miry-b.raw', 'cen1-marh-b.raw', 'an431-marh-b.raw', 'an421-menk-b.raw', 'cen2-mjgm-b.raw', 'an435-marh-b.raw', 'an397-miry-b.raw', 'cen5-marh-b.raw', 'an425-menk-b.raw', 'an436-mjgm-b.raw', 'cen6-mjgm-b.raw', 'an426-fvap-b.raw', 'an444-mmxg-b.raw', 'an403-mdms2-b.raw', 'cen3-mdms2-b.raw', 'cen2-fjlp-b.raw', 'cen1-fvap-b.raw', 'cen4-menk-b.raw', 'an417-fjlp-b.raw', 'cen5-fvap-b.raw', 'cen6-fjlp-b.raw', 'cen8-menk-b.raw', 'cen1-mjwl-b.raw', 'cen4-mmxg-b.raw', 'cen5-mjwl-b.raw', 'cen7-fvap-b.raw', 'cen8-fjlp-b.raw', 'cen2-mmxg-b.raw', 'cen3-mjwl-b.raw', 'cen6-mmxg-b.raw', 'cen7-mjwl-b.raw', 'an394-mjwl-b.raw', 'cen3-miry-b.raw']),

#        SPECRun('999.specrand', 'specrand', '1255432124 234923', '', 'rand.234923.out', 'rand.234923.err', 'rand.234923')
]


def GetRun(name):
    res = None
    for curr_run in runs:
        if curr_run.name == name:
            res = curr_run
            break
    if res == None:
        raise ValueError('No benchmark %s in %s' % (name, __name__))
    return res
