# dashboard-server
Middleware server. Serves embeddingstore data via HTTP following connection with embeddinghub via grpc
## Specification:
Dockerized server which provides a blank interface to serve data efficiently to the frontend. Acts as a middleware between the feature store backend and the frontend dashboard. Can provide logs for usage and potential CRUD functionality for container features for the feature store.

### Build Docker image:
`docker build -t api .`

### Run Docker container:
`docker run -it -p 8080:8080 api`