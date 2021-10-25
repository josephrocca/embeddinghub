package router

import (
	"github.com/gin-gonic/gin"

	"github.com/Sami1309/go-grpc-server/middleware"
)

func Router() *gin.Engine {
	//Initialize gin router
	router := gin.Default()

	//Retrieve list of spaces in embeddingstore
	router.GET("/spaces", middleware.GetSpaces)

	//Retrieve information about individual space
	router.GET("/spaces/:name", middleware.GetEmbeddings)

	//Get specific embedding
	router.GET("/spaces/:name/:key", middleware.GetEmbedding)

	//Get nearest neighbors for embedding
	router.GET("/spaces/:name/:key/*nn", middleware.GetNearestNeighbors)

	return router

}
