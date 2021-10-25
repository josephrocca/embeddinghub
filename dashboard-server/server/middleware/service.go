package middleware

import (
	//remote packages
	"context"
	"io"
	"log"
	"net/http"
	"strconv"
	"time"

	pb "github.com/Sami1309/go-grpc-server/grpc"
	"github.com/gin-gonic/gin"
	"google.golang.org/grpc"
)

//API: router.GET("/spaces", middleware.GetSpaces)
func GetSpaces(c *gin.Context) {
	conn, err := grpc.Dial(address, grpc.WithInsecure(), grpc.WithBlock())
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}

	CLIENT := pb.NewEmbeddingHubClient(conn)

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()
	stream, getSpacesError := CLIENT.DownloadSpaces(ctx, &pb.DownloadSpacesRequest{})
	if getSpacesError != nil {
		log.Fatalf("Error message: ( %v)", getSpacesError)
	}

	var space_list []string

	done := make(chan bool)

	go func() {
		for {
			resp, err := stream.Recv()
			if err == io.EOF {
				done <- true //means stream is finished
				return
			}
			if err != nil {
				log.Fatalf("cannot receive %v", err)
			}
			space_list = append(space_list, resp.GetSpace())
			log.Printf("Space received: %s", resp.GetSpace())
		}
	}()

	<-done //we will wait until all response is received
	log.Printf("finished")
	c.JSON(http.StatusOK, gin.H{"Spaces": space_list})
}

//API: router.GET("/spaces/:name/:key", middleware.GetEmbedding)
func GetEmbedding(c *gin.Context) {
	conn, err := grpc.Dial(address, grpc.WithInsecure(), grpc.WithBlock())
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}

	CLIENT := pb.NewEmbeddingHubClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	name := c.Param("name")
	key := c.Param("key")
	getResponse, getResponseErr := CLIENT.Get(ctx, &pb.GetRequest{Key: key, Space: name})
	if getResponseErr != nil {
		log.Fatalf("Error message: ( %v)", getResponseErr)
	}

	log.Printf("Retrieved vector: %s", getResponse.GetEmbedding())
	c.JSON(http.StatusOK, gin.H{"Space": getResponse.GetEmbedding()})

}

//API: router.GET("/spaces/:name", middleware.GetSpaceVectors)
func GetEmbeddings(c *gin.Context) {
	conn, err := grpc.Dial(address, grpc.WithInsecure(), grpc.WithBlock())
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}

	CLIENT := pb.NewEmbeddingHubClient(conn)

	name := c.Param("name")

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()
	stream, downloadError := CLIENT.Download(ctx, &pb.DownloadRequest{Space: name})
	if downloadError != nil {
		log.Fatalf("Error message: ( %v)", downloadError)
	}

	var embedding_list []string

	done := make(chan bool)

	go func() {
		for {
			resp, err := stream.Recv()
			if err == io.EOF {
				done <- true //means stream is finished
				return
			}
			if err != nil {
				log.Fatalf("cannot receive %v", err)
			}
			embedding_list = append(embedding_list, resp.String())
			log.Printf("Embedding received: %s", resp.String())
		}
	}()

	<-done //we will wait until all response is received
	log.Printf("finished")
	c.JSON(http.StatusOK, gin.H{"Embeddings": embedding_list})
}

//API: router.GET("/spaces/:name/:key/*nn?num=<num_value>", middleware.GetNearestNeighbors)
func GetNearestNeighbors(c *gin.Context) {
	conn, err := grpc.Dial(address, grpc.WithInsecure(), grpc.WithBlock())
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}

	CLIENT := pb.NewEmbeddingHubClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	name := c.Param("name")
	key := c.Param("key")
	num, err := strconv.ParseInt(c.Query("num"), 10, 64)
	if err != nil {
		log.Fatalf("improper number format: %v", err)
	}
	getResponse, getResponseErr := CLIENT.NearestNeighbor(ctx, &pb.NearestNeighborRequest{Key: key, Space: name, Num: int32(num), Embedding: nil})

	if getResponseErr != nil {
		log.Fatalf("Error message: ( %v)", getResponseErr)
	}

	log.Printf("Nearest neighbors: %s", getResponse.GetKeys())
	c.JSON(http.StatusOK, gin.H{"Nearest neighbors": getResponse.GetKeys()})

}
