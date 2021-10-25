package main

import (
	"github.com/Sami1309/go-grpc-server/middleware"
	"github.com/Sami1309/go-grpc-server/router"
)

func main() {

	r := router.Router()

	middleware.ConnectGRPC()

	//myport := fmt.Sprintf(":%s", os.Getenv("PORT"))

	r.Run(":8080")
}
