import os
from django.http import Http404, HttpResponse
from django.shortcuts import render_to_response
from django.utils.encoding import smart_str

report_fn  = '../quast_results_archive_json/latest/report.json'
contigs_fn = '../quast_results_archive_json/latest/contigs.json'

def latestreport(request):
    try:
        report = open(report_fn).read()
    except IOError:
        raise Http404

    try:
        contigs = open(contigs_fn).read()
    except IOError:
        raise Http404

    return render_to_response('latest-report.html', {
        'report'  : report,
        'contigs' : contigs
    })

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