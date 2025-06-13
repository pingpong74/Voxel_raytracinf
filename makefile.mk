file := Src/main.cpp Src/application.cpp Src/RayTracing/raytracer.cpp 
shaders := Shaders/intersection.rint Shaders/closestHit.rchit Shaders/miss.rmiss Shaders/raygen.rgen

cFlags := -std=c++17 -O2
ldFlags := -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

application: $(file) $(shaders)
	rm -f application Shaders/raygen.spv Shaders/closestHit.spv Shaders/miss.spv Shaders/intersection.spv
	glslc --target-spv=spv1.5 Shaders/raygen.rgen -o Shaders/raygen.spv
	glslc --target-spv=spv1.5 Shaders/closestHit.rchit -o Shaders/closestHit.spv
	glslc --target-spv=spv1.5 Shaders/miss.rmiss -o Shaders/miss.spv
	glslc --target-spv=spv1.5 Shaders/intersection.rint -o Shaders/intersection.spv
	g++ $(cFlags) -o application $(file) $(ldFlags)

clean: 
	rm -f application Shaders/raygen.spv Shaders/closestHit.spv Shaders/miss.spv Shaders/intersection.spv