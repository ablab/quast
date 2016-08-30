from local_pkg_resources.extern import VendorImporter


names = 'six',
VendorImporter(__name__, names, 'local_pkg_resources._vendor').install()
