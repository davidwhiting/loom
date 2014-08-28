# Copyright (c) 2014, Salesforce.com, Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# - Neither the name of Salesforce.com nor the names of its contributors
#   may be used to endorse or promote products derived from this
#   software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
# USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import csv
import mock
from nose.tools import raises
from distributions.io.stream import open_compressed
from loom.util import LoomError, tempdir
from loom.test.util import for_each_dataset, CLEANUP_ON_ERROR
import loom.store
import loom.format
import loom.datasets
import loom.tasks


def csv_load(filename):
    with open_compressed(filename) as f:
        reader = csv.reader(f)
        return list(reader)


def csv_dump(data, filename):
    with open_compressed(filename, 'w') as f:
        writer = csv.writer(f)
        for row in data:
            writer.writerow(row)


@for_each_dataset
@raises(LoomError)
def test_csv_missing_header(name, schema, encoding, rows, **unused):
    with tempdir(cleanup_on_error=CLEANUP_ON_ERROR):
        with mock.patch('loom.store.STORE', new=os.getcwd()):
            rows_dir = os.path.abspath('rows_csv')
            loom.format.export_rows(encoding, rows, rows_dir)
            rows_csv = os.path.join(rows_dir, os.listdir(rows_dir)[0])
            data = csv_load(rows_csv)
            data = data[1:]
            csv_dump(data, rows_csv)
            loom.tasks.ingest(name, schema, rows_csv)