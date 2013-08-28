# quast Developer Readme

## Running this app with additional computational resources

This app has the following entry points:

* main

When running this app, you can override the instance type to be used by
providing the ``systemRequirements`` field to ```/applet-XXXX/run``` or
```/app-XXXX/run```, as follows:

    {
      systemRequirements: {
        "main": {"instanceType": "dx_m1.large"}
      },
      [...]
    }

See <a
href="http://wiki.dnanexus.com/API-Specification-v1.0.0/IO-and-Run-Specifications#Run-Specification">Run
Specification</a> in the API documentation for more information about the
available instance types.

## Reporting bugs

We will be thankful if you help us make QUAST better by sending your comments, bug reports, and 
suggestions to <a href="mailto:quast.support@bioinf.spbau.ru">quast.support@bioinf.spbau.ru</a>.
