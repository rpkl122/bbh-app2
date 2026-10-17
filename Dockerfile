FROM alpine:3.20
RUN apk add --no-cache python3 curl bash util-linux
COPY server.py /server.py
EXPOSE 80
CMD ["python3","/server.py"]
