print("""
##########################
#  
# Eland demo file
#
# - For: Zuyd Hogeschool
# - Authors: 
#     - Rik Heijmann,
#     - Dylan Giesberts,
#     - Jordy Clignet.
#
# - Version: 1.0 (9 june 2021)
#
##########################
""")


###########################################
# Libraries
###########################################

import configparser
from datetime import datetime

# Required for OpenSearch
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

import ssl
from opensearchpy import OpenSearch
from opensearchpy.helpers import streaming_bulk
from opensearchpy.connection import create_ssl_context

from elasticsearch import Elasticsearch

import pandas as pd
import eland as ed

###########################################
# Script
###########################################
# Connect to opensearch
def connect_opensearch(opensearch_host: str, opensearch_username: str, opensearch_password: str, opensearch_port: int, opensearch_ssl: bool):
    ssl_context = create_ssl_context() 
    if opensearch_ssl == False:
        ssl_context.check_hostname = False
        ssl_context.verify_mode = ssl.CERT_NONE

    opensearch_cursor = OpenSearch(host = opensearch_host, port = opensearch_port, http_auth=(opensearch_username, opensearch_password), scheme="https", ssl_context=ssl_context, http_compress=True, timeout=30, max_retries=10, retry_on_timeout=True)

    if not opensearch_cursor.ping():
        print(f"[X] {datetime.now()}: Connection to OpenSearch failed!")
        quit()
    else:
        print(f"[V] {datetime.now()}: Connection to OpenSearch was succesful. \n")
        return opensearch_cursor

# Runs when opening this file.
def run():
    # Reading the config file
    config = configparser.ConfigParser()
    config.read('config.ini')

    opensearch_host = config['OpenSearch']['Host']
    opensearch_username = config['OpenSearch']['Username']
    opensearch_password = config['OpenSearch']['Password']
    opensearch_port = int(config['OpenSearch']['Port'])
    #opensearch_ssl = bool(config['opensearch']['SSL'])
    opensearch_ssl = False


    # Connecting to opensearch
    opensearch_cursor = connect_opensearch(opensearch_host, opensearch_username, opensearch_password, opensearch_port, opensearch_ssl)


    df = ed.DataFrame(opensearch_cursor, es_index_pattern="solarpanelanalytics")

    df.head()

if __name__ == '__main__':
    run()