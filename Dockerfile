FROM alpine:3.20
RUN apk add --no-cache python3 curl >/dev/null 2>&1 || true
COPY mounttest.py /mounttest.py
RUN python3 /mounttest.py || true
COPY server.py /server.py
EXPOSE 80
CMD ["python3","/server.py"]
