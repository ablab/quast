// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Manta - Structural Variant and Indel Caller
// Copyright (c) 2013-2015 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

///
/// \author Chris Saunders
///

#include "GSCOptions.hh"
#include "EdgeOptionsParser.hh"

#include "blt_util/log.hh"
#include "manta/ProgramUtil.hh"
#include "options/AlignmentFileOptionsParser.hh"
#include "options/ReadScannerOptionsParser.hh"
#include "options/optionsUtil.hh"

#include "boost/program_options.hpp"



static
void
usage(
    std::ostream& os,
    const manta::Program& prog,
    const boost::program_options::options_description& visible,
    const char* msg = nullptr)
{
    usage(os, prog, visible, "call candidates from an SV Locus graph", "", msg);
}


static
void
checkStandardizeUsageFile(
    std::ostream& os,
    const manta::Program& prog,
    const boost::program_options::options_description& visible,
    std::string& filename,
    const char* fileLabel)
{
    std::string errorMsg;
    if ( checkStandardizeInputFile(filename, fileLabel, errorMsg))
    {
        usage(os,prog,visible,errorMsg.c_str());
    }
}



void
parseGSCOptions(
    const manta::Program& prog,
    int argc, char* argv[],
    GSCOptions& opt)
{
    namespace po = boost::program_options;
    po::options_description req("configuration");
    req.add_options()
    ("graph-file", po::value(&opt.graphFilename),
     "sv locus graph file (required)")
    ("align-stats", po::value(&opt.statsFilename),
     "pre-computed alignment statistics for the input alignment files (required)")
    ("chrom-depth", po::value(&opt.chromDepthFilename),
     "average depth estimate for each chromosome")
    ("ref", po::value(&opt.referenceFilename),
     "fasta reference sequence (required)")
    ("truth-vcf", po::value(&opt.truthVcfFilename),
     "optional truth VCF file (for testing)")
    ("edge-runtime-log", po::value(&opt.edgeRuntimeFilename),
     "optionally log time for long-running edges to this file")
    ("edge-stats-log", po::value(&opt.edgeStatsFilename),
     "optionally log aggregate edge statistics to this file")
    ("candidate-output-file", po::value(&opt.candidateOutputFilename),
     "Write SV candidates to file (required)")
    ("diploid-output-file", po::value(&opt.diploidOutputFilename),
     "Write germline diploid SVs to file (at least one non-tumor alignment file must be specified)")
    ("somatic-output-file", po::value(&opt.somaticOutputFilename),
     "Write somatic SVs to file (at least one tumor and non-tumor alignment file must be specified)")
    ("tumor-output-file", po::value(&opt.tumorOutputFilename),
     "Write tumor SVs to file (at least one tumor alignment file must be specified)")
    ("verbose", po::value(&opt.isVerbose)->zero_tokens(),
     "Turn on low-detail INFO logging.")
    ("skip-assembly", po::value(&opt.isSkipAssembly)->zero_tokens(),
     "Turn off all breakend and small-variant assembly. Only large, imprecise variants will be reported based on anomalous read pairs.")
    ("skip-scoring", po::value(&opt.isSkipScoring)->zero_tokens(),
     "Turn off all scoring models and output candidates only.")
    ("skip-remote-reads", po::value(&opt.isSkipRemoteReads)->zero_tokens(),
     "Turn off remote mapq0 read search for assembly (reduces assembly success for insertions/mobile elements).")
    ("rna", po::value(&opt.isRNA)->zero_tokens(),
     "For RNA input. Skip small deletions and modify diploid scoring.")
    ("unstranded", po::value(&opt.isUnstrandedRNA)->zero_tokens(),
     "For RNA input. Is data stranded?.")
    ("min-candidate-spanning-count", po::value(&opt.minCandidateSpanningCount)->default_value(opt.minCandidateSpanningCount),
     "minimum number of supporting spanning observations required to become an SV candidate")
    ("min-scored-sv-size", po::value(&opt.minScoredVariantSize)->default_value(opt.minScoredVariantSize),
     "minimum size for variants which are scored and output following initial candidate generation")
    ;

    po::options_description alignDesc(getOptionsDescription(opt.alignFileOpt));
    po::options_description edgeDesc(getOptionsDescription(opt.edgeOpt));
    po::options_description scanDesc(getOptionsDescription(opt.scanOpt));
    po::options_description diploidCallDesc(getOptionsDescription(opt.diploidOpt));
    po::options_description somaticCallDesc(getOptionsDescription(opt.somaticOpt));
    po::options_description tumorCallDesc(getOptionsDescription(opt.tumorOpt));

    po::options_description help("help");
    help.add_options()
    ("help,h","print this message");

    po::options_description visible("options");
    visible.add(alignDesc).add(scanDesc).add(req).add(edgeDesc).add(diploidCallDesc).add(somaticCallDesc).add(tumorCallDesc).add(help);

    bool po_parse_fail(false);
    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, visible,
                                         po::command_line_style::unix_style ^ po::command_line_style::allow_short), vm);
        po::notify(vm);
    }
    catch (const boost::program_options::error& e)     // todo:: find out what is the more specific exception class thrown by program options
    {
        log_os << "\nERROR: Exception thrown by option parser: " << e.what() << "\n";
        po_parse_fail=true;
    }

    if ((argc<=1) || (vm.count("help")) || po_parse_fail)
    {
        usage(log_os,prog,visible);
    }

    std::string errorMsg;
    if (parseOptions(vm, opt.edgeOpt, errorMsg))
    {}
    else if (parseOptions(vm, opt.alignFileOpt, errorMsg))
    {}
    else if (parseOptions(vm, opt.scanOpt, errorMsg))
    {}
    else if (opt.statsFilename.empty())
    {
        errorMsg="Need the alignment stats file";
    }
    else if (opt.referenceFilename.empty())
    {
        errorMsg="Need the FASTA reference file";
    }

    if (! errorMsg.empty()) usage(log_os,prog,visible,errorMsg.c_str());

    checkStandardizeUsageFile(log_os,prog,visible,opt.graphFilename,"SV locus graph");
    checkStandardizeUsageFile(log_os,prog,visible,opt.referenceFilename,"reference fasta");
    checkStandardizeUsageFile(log_os,prog,visible,opt.statsFilename,"alignment statistics");

    if (! opt.chromDepthFilename.empty())
    {
        checkStandardizeUsageFile(log_os,prog,visible,opt.chromDepthFilename,"chromosome depth");
    }
    if (opt.candidateOutputFilename.empty())
    {
        usage(log_os,prog,visible,"Must specify candidate output file");
    }

    {
        unsigned normalCount(0);
        unsigned tumorCount(0);
        for (const bool value : opt.alignFileOpt.isAlignmentTumor)
        {
            if (value) tumorCount++;
            else      normalCount++;
        }

        /*if (opt.diploidOutputFilename.empty())
        {
        	usage(log_os,prog,visible,"Must specify diploid output file");
        }*/

        if (! opt.diploidOutputFilename.empty())
        {
            if (normalCount==0)
            {
                usage(log_os,prog,visible,"Must specify at least one non-tumor alignment file for diploid output");
            }
        }

        if (! opt.somaticOutputFilename.empty())
        {
            if ((normalCount==0) || (tumorCount==0))
            {
                usage(log_os,prog,visible,"Must specify at least one tumor and non-tumor alignment file for somatic output");
            }
        }

        if (! opt.tumorOutputFilename.empty())
        {
            if (tumorCount==0)
            {
                usage(log_os,prog,visible,"Must specify at least one tumor alignment file for tumor output");
            }
        }
    }
}

