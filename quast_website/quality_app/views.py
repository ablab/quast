from django.http import Http404, HttpResponse
from django.shortcuts import render_to_response
from django.utils.encoding import smart_str

report_fn  = '../quast/latest_json/report.json'
lengths_fn = '../quast/latest_json/lengths.json'

def latest_report(request):
    try:
        report = open(report_fn).read()
    except IOError:
        raise Http404

    try:
        lengths = open(lengths_fn).read()
    except IOError:
        raise Http404

    return render_to_response('latest-report.html', {
        'report'  : report,
        'lengths' : lengths
    })


#static_path = 'quality_app/static/'
#
#def get_static_file(request, path):
#    try:
#        contents = open(static_path + path)
#    except IOError:
#        return ''
#    else:
#        return contents


def manual(request):
    path = '../quast/manual.html'

    response = HttpResponse(mimetype='application/force-download')
    response['Content-Disposition'] = 'attachment; filename=%s' % 'manual.html'
    response['X-Sendfile'] = path

    return response

def tar_archive(request):
    path = '../quast/quast.tar.gz'

    response = HttpResponse(mimetype='application/force-download')
    response['Content-Disposition'] = 'attachment; filename=%s' % 'quast.tar.gz'
    response['X-Sendfile'] = path

    return response
