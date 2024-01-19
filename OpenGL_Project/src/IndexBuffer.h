#pragma once
class IndexBuffer
{
private:
	unsigned int m_RendererID;
	unsigned int m_Count;
public:
	IndexBuffer(const unsigned int* data, unsigned int count);//count means element count, not bytes
	~IndexBuffer();

	void Bind() const;
	void Unbind() const;
	
	inline unsigned int GetCount() const { return m_Count; }
};

