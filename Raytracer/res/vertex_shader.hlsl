struct VS_OUT {
	float2 tc : TexCoord;
	float4 pos : SV_Position;
};

VS_OUT main(float2 pos : Position, float2 tc : TexCoord) {
	VS_OUT vs_out;
	vs_out.pos = float4(pos.x, pos.y, 0, 1);
	vs_out.tc = tc;
	return vs_out;
}

