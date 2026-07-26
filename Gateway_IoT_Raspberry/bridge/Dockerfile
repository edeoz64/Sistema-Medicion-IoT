FROM python:3.9-slim

WORKDIR /app


RUN pip install paho-mqtt pyserial requests


COPY . .


CMD ["python", "-u", "puente.py"]
