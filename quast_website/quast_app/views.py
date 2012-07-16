import os
import subprocess
from django.http import Http404, HttpResponse
from django.shortcuts import render_to_response
from django.utils.encoding import smart_str
from django import forms

defalt_dir = '../quast_results_archive_json/latest/'

report_fn  =            'report.json'
contigs_fn =            'contigs_lengths.json'
aligned_contigs_fn =    'aligned_contigs_lengths.json'
assemblies_lengths_fn = 'assemblies_lengths.json'
reference_length_fn =   'ref_length.json'


def response_with_report(template, dir):
    try:
        report = open(dir + report_fn).read()
    except IOError:
        raise Http404

    try:
        contigs_lengths = open(dir + contigs_fn).read()
    except IOError:
        raise Http404

    try:
        assemblies_lengths = open(dir + assemblies_lengths_fn).read()
    except IOError:
        raise Http404

    try:
        aligned_contigs_lengths = open(dir + aligned_contigs_fn).read()
    except IOError:
        pass

    try:
        reference_length = open(dir + reference_length_fn).read()
    except IOError:
        pass

    return render_to_response(template, {
        'report' : report,
        'contigsLenghts' : contigs_lengths,
        'alignedContigsLengths' : aligned_contigs_lengths,
        'assembliesLengths' : assemblies_lengths,
        'referenceLength' : reference_length,
    })


def latestreport(request):
    return response_with_report('latest-report.html', defalt_dir)

#static_path = 'quast_app/static/'
#
#def get_static_file(request, path):
#    try:
#        contents = open(static_path + path)
#    except IOError:
#        return ''
#    else:
#        return HttpResponse(contents)

def manual(request):
    try:
        contents = open('../manual.html')
    except IOError:
        raise Http404
    else:
        return HttpResponse(contents)

#def tar_archive(request, version):
#    path = '/Users/vladsaveliev/Dropbox/bio/quast/quast_website/quast' + version + '.tar.gz'
#
#    if os.path.isfile(path):
#        response = HttpResponse(mimetype='application/x-gzip')
#        response['Content-Disposition'] = 'attachment; filename=quast' + version +'.tar.gz'
#        response['X-Sendfile'] = path
#        return response
#    else:
#        raise Http404

def index(request):
    return render_to_response('index.html')



def assess_with_quast(files):
    contigs = [files['contigs1']]
    reference = files['reference']
    genes = files['genes']
    operons = files['operons']
    if contigs:
        if reference and operons and genes:
            subprocess.call('../quast.py --save-archive -R test/reference.fa.gz -G test/genes.txt '
                + '-O test/operons.txt test/allpaths_full_ecoli.fasta test/SPAdes_full_ecoli.fasta')
            response_with_report('assess-report.html', defalt_dir)


class UploadForm(forms.Form):
    contigs1    = forms.FileField()
    reference   = forms.FileField()
    genes       = forms.FileField()
    operons     = forms.FileField()


def assess(request):
    if request.method == 'POST':
        form = UploadForm(request.POST, request.FILES)
        if form.is_valid():
            results_dir = assess_with_quast(request.FILES)
            return response_with_report('assess-report.html', results_dir)
        else:
            raise Http404

    else:
        form = UploadForm()
    return render_to_response('assess.html', {'form' : form})















